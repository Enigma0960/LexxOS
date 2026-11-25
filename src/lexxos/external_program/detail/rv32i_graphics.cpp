#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/guest_graphics_mode.h>
#include <lexxos/app/program_input.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/bus/spi_transfer.h>
#include <lexxos/devices/bus/uart_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/external_program/lexx_graphics_asset.h>
#include <lexxos/external_program/lexx_resources.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/ui/guest_ui.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

#include <lexxos/external_program/detail/rv32i_support.h>
#include <lexxos/external_program/detail/rv32i_virtual_machine.h>

namespace lexxos::external_program::detail {

namespace {

	// Timings use the board's monotonic clock and never allocate on the fast path.
	// Independent buckets prevent a slow LCD frame from hiding a slow SD read.
	class ScopedGraphicsTiming {
	public:
		ScopedGraphicsTiming(lexxos::os::AppContext& context,
		    std::uint32_t& last, bool& reported, char const* phase,
		    std::uint32_t limit)
		    : _context(context)
		    , _last(last)
		    , _reported(reported)
		    , _phase(phase)
		    , _limit(limit)
		    , _clock(lexxos::platform::services().clock.uptimeMs)
		    , _started(_clock != nullptr ? _clock() : 0xFFFFFFFFU) {}

		~ScopedGraphicsTiming() {
			if (_clock == nullptr || _started == 0xFFFFFFFFU) {
				return;
			}

			std::uint32_t const now = _clock();
			if (now == 0xFFFFFFFFU) {
				return;
			}

			std::uint32_t const elapsed = now - _started;
			if (elapsed <= _limit || (_reported && now - _last < 5000U)) {
				return;
			}

			_reported = true;
			_last = now;
			std::array<char, 160> line{};
			std::size_t pos = 0U;
			append(line, pos, "lexx-graphics PERF phase=");
			append(line, pos, _phase);
			append(line, pos, " elapsed=");
			decimal(line, pos, elapsed);
			append(line, pos, "ms limit=");
			decimal(line, pos, _limit);
			append(line, pos, "ms");
			_context.writeErrorLine(std::string_view{line.data(), pos});
		}

	private:
		static void append(std::array<char, 160>& output, std::size_t& pos, char const* source) {
			while (*source != '\0' && pos + 1U < output.size()) {
				output[pos++] = *source++;
			}
			output[pos] = '\0';
		}

		static void decimal(std::array<char, 160>& output, std::size_t& pos, std::uint32_t value) {
			char digits[10]{};
			std::size_t used = 0U;
			do {
				digits[used++] = static_cast<char>('0' + value % 10U);
				value /= 10U;
			} while (value != 0U);
			while (used != 0U && pos + 1U < output.size()) {
				output[pos++] = digits[--used];
			}

			output[pos] = '\0';
		}

		lexxos::os::AppContext& _context;
		std::uint32_t& _last;
		bool& _reported;
		char const* _phase;
		std::uint32_t _limit;
		std::uint32_t (*_clock)();
		std::uint32_t _started;
	};

} // namespace

void Rv32iVirtualMachine::releaseGraphics() {
	if (_uiSession.owner() != 0U) {
		lexxos::ui::UiService::close(_context, _uiSession);
		_graphicsEnabled = false;
		return;
	}

	if (!_graphicsEnabled) {
		return;
	}

	_graphicsEnabled = false;
	lexxos::app::guest_graphics_mode::release(_context.processId());
	if (_context.hasProgramFocus()) {
		lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
		lexxos::app::console_mirror::refresh();
	}
}

bool Rv32iVirtualMachine::graphicsTextValid(std::uint32_t address, std::uint32_t& length) const {
	length = 0;
	if (!rangeValid(address, 1U)) {
		return false;
	}

	for (; length < lexxGraphicsMaxText; ++length) {
		if (length >= _memorySize - address) {
			return false;
		}

		if (_memory[address + length] == 0) {
			return true;
		}
	}

	return false;
}

bool Rv32iVirtualMachine::drawGraphicsText(lexxos::devices::DisplayDevice& display, int x, int y,
    std::uint32_t address, int scale, std::uint32_t colors, bool transparent) {
	std::uint32_t length = 0;
	if (!graphicsTextValid(address, length)) {
		return false;
	}

	constexpr int maxScanPixels = 320;
	int const linePixels = std::min(static_cast<int>(length) * 6 * scale,
	    std::min(maxScanPixels, display.width() - x));
	if (linePixels <= 0) {
		return true;
	}

	std::array<std::uint16_t, maxScanPixels> scan{};
	std::uint16_t const ink = static_cast<std::uint16_t>(colors);
	std::uint16_t const paper = static_cast<std::uint16_t>(colors >> 16U);
	for (int row = 0; row < 7 * scale; ++row) {
		int runStart = -1;
		for (int column = 0; column < linePixels; ++column) {
			int const character = column / (6 * scale);
			int const glyphColumn = (column / scale) % 6;
			std::uint8_t const bits = glyphColumn < 5
			    ? graphics_asset::glyphColumn(static_cast<char>(_memory[address + character]), glyphColumn)
			    : 0U;
			bool const painted = (bits & (1U << (row / scale))) != 0U;
			if (!transparent) {
				scan[column] = painted ? ink : paper;
			} else if (painted && runStart < 0) {
				runStart = column;
			} else if (!painted && runStart >= 0) {
				if (!display.fillRgb565({x + runStart, y + row, column - runStart, 1}, ink)) {
					return false;
				}

				runStart = -1;
			}
		}

		if (transparent) {
			if (runStart >= 0
			    && !display.fillRgb565({x + runStart, y + row, linePixels - runStart, 1}, ink)) {
				return false;
			}
		} else if (!display.blitRgb565({x, y + row, linePixels, 1}, scan.data(), linePixels)) {
			return false;
		}
	}

	return true;
}

bool Rv32iVirtualMachine::graphicsRectValid(lexxos::devices::DisplayDevice const& display,
    int x, int y, int width, int height) const {
	return x >= 0 && y >= 0 && width > 0 && height > 0 && x < display.width() && y < display.height() && width <= display.width() - x && height <= display.height() - y;
}

void Rv32iVirtualMachine::loadGraphicsImage(std::uint32_t guestPathAddress, bool streamed) {
	ScopedGraphicsTiming slow{_context, _graphicsSlowLast[streamed ? 1U : 0U],
	    _graphicsSlowReported[streamed ? 1U : 0U],
	    streamed ? "load.stream" : "load.cached", 75U};
	_registers[10] = deviceDenied;
	if (!_context.hasProgramFocus()) {
		return;
	}

	_registers[10] = deviceInvalid;
	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!loadGuestString(guestPathAddress, path) || !std::string_view{path.data()}.starts_with(":/")) {
		return;
	}
	std::size_t slot = _graphicsImages.size();
	for (std::size_t i = 0; i < _graphicsImages.size(); ++i) {
		if (_graphicsImages[i].pixels == nullptr && !_graphicsImages[i].streamed) {
			slot = i;
			break;
		}
	}

	_registers[10] = deviceNoHandles;
	if (slot == _graphicsImages.size()) {
		return;
	}

	lexxos::vfs::FileHandle package{};
	_registers[10] = deviceNotFound;
	if (!_resourceIndex.present || _context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, package) != lexxos::vfs::OpenStatus::Ok) {
		return;
	}

	resources::Entry entry{};
	if (!resources::find(package, _resourceIndex, path.data(), entry)) {
		return;
	}

	_registers[10] = deviceInvalid;
	std::uint8_t header[graphics_asset::headerBytes]{};
	if (entry.size < sizeof(header) || !package.seek(entry.offset) || !resources::readExact(package, header, sizeof(header))) {
		return;
	}

	constexpr auto policy = runtime::standardRuntimeProfile.graphics;
	graphics_asset::Info info{};
	if (!graphics_asset::decodeHeader(header, entry.size, info,
	        policy.maxImagePixels, policy.maxImageDimension)) {
		return;
	}

	std::uint32_t const pixelCount = static_cast<std::uint32_t>(info.width) * info.height;
	std::uint32_t const payloadStart = entry.offset + graphics_asset::headerBytes;
	_registers[10] = deviceNotReady;

	if (streamed) {
		// A reusable bounded staging allocation, never one framebuffer per asset.
		if (!_graphicsStreamScratch) {
			_graphicsStreamScratch.reset(new (std::nothrow) std::uint8_t[policy.streamChunkBytes]);
			if (!_graphicsStreamScratch) {
				return;
			}
		}

		_registers[10] = deviceIoError;
		std::uint32_t crc = 0xFFFFFFFFU;
		std::uint32_t remaining = info.payloadBytes;
		while (remaining != 0U) {
			auto const n = static_cast<std::size_t>(std::min<std::uint32_t>(
			    remaining, static_cast<std::uint32_t>(policy.streamChunkBytes)));
			if (!resources::readExact(package, _graphicsStreamScratch.get(), n)) {
				return;
			}

			crc = graphics_asset::crc32Update(crc, _graphicsStreamScratch.get(), n);
			remaining -= static_cast<std::uint32_t>(n);
		}

		if ((~crc) != info.crc32) {
			return;
		}

		_graphicsImages[slot] = {nullptr, nullptr, info.width, info.height,
		    0U, payloadStart, true, info.hasAlpha};
	} else {
		std::size_t const bytes = info.payloadBytes + (info.payloadBytes & 1U);
		_registers[10] = deviceNoHandles;
		if (bytes > policy.imageCacheBytes || _graphicsCacheBytes > policy.imageCacheBytes - bytes) {
			return;
		}

		auto* pixels = new (std::nothrow) std::uint16_t[bytes / 2U];
		_registers[10] = deviceNotReady;
		if (pixels == nullptr) {
			return;
		}

		_registers[10] = deviceIoError;
		if (!resources::readExact(package, pixels, info.payloadBytes) || !graphics_asset::crcMatches(reinterpret_cast<std::uint8_t const*>(pixels), info.payloadBytes, info.crc32)) {
			delete[] pixels;
			return;
		}

		auto const* alpha = info.hasAlpha
		    ? reinterpret_cast<std::uint8_t const*>(pixels + pixelCount)
		    : nullptr;
		_graphicsImages[slot] = {pixels, alpha, info.width, info.height,
		    static_cast<std::uint32_t>(bytes), payloadStart, false, info.hasAlpha};
		_graphicsCacheBytes += bytes;
	}

	_registers[10] = static_cast<std::uint32_t>(slot + 1U);
}

bool Rv32iVirtualMachine::drawStreamedGraphicsImage(
    lexxos::devices::DisplayDevice& display, GuestGraphicsImage const& image,
    int x, int y, int sx, int sy, int width, int height) {
	ScopedGraphicsTiming slow{_context, _graphicsSlowLast[4], _graphicsSlowReported[4],
	    "image.stream", 80U};
	if (!_graphicsStreamScratch || width <= 0 || height <= 0) {
		return false;
	}

	lexxos::vfs::FileHandle file{};
	if (_context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, file)
	    != lexxos::vfs::OpenStatus::Ok) {
		return false;
	}

	constexpr auto policy = runtime::standardRuntimeProfile.graphics;
	std::size_t const pixelsPerChunk = policy.streamChunkBytes / (image.hasAlpha ? 3U : 2U);
	if (pixelsPerChunk == 0U) {
		return false;
	}

	std::uint32_t const totalPixels = static_cast<std::uint32_t>(image.width) * image.height;
	for (int row = 0; row < height; ++row) {
		for (int column = 0; column < width;) {
			int const n = std::min(width - column, static_cast<int>(pixelsPerChunk));
			std::uint32_t const pixel = static_cast<std::uint32_t>(sy + row) * image.width + sx + column;
			if (!file.seek(static_cast<std::uint64_t>(image.resourceOffset) + pixel * 2U) || !resources::readExact(file, _graphicsStreamScratch.get(), n * 2U)) {
				return false;
			}

			std::uint8_t const* alpha = nullptr;
			if (image.hasAlpha) {
				alpha = _graphicsStreamScratch.get() + n * 2U;
				if (!file.seek(static_cast<std::uint64_t>(image.resourceOffset) + totalPixels * 2U + pixel) || !resources::readExact(file, _graphicsStreamScratch.get() + n * 2U, n)) {
					return false;
				}
			}

			lexxos::devices::Rect const rect{x + column, y + row, n, 1};
			auto const* rgb = reinterpret_cast<std::uint16_t const*>(_graphicsStreamScratch.get());
			bool const ok = image.hasAlpha
			    ? display.blitRgb565Alpha(rect, rgb, alpha, n)
			    : display.blitRgb565(rect, rgb, n);
			if (!ok) {
				return false;
			}

			column += n;
		}
	}

	return true;
}

void Rv32iVirtualMachine::unloadGraphicsImage(std::uint32_t handle) {
	_registers[10] = deviceInvalid;
	if (handle == 0U || handle > _graphicsImages.size() || (_graphicsImages[handle - 1U].pixels == nullptr && !_graphicsImages[handle - 1U].streamed)) {
		return;
	}

	auto const& image = _graphicsImages[handle - 1U];
	_graphicsCacheBytes -= image.payloadBytes;
	delete[] image.pixels;

	_graphicsImages[handle - 1U] = {};
	_registers[10] = 0U;
}

void Rv32iVirtualMachine::handleSystemUi(std::uint32_t operation, std::uint32_t address, std::uint32_t bytes) {
	_registers[10] = deviceDenied;
	if (!hasCapability(LexxCapabilityGraphics)) {
		return;
	}

	if (operation == 8U) {
		releaseGraphics();
		_registers[10] = 0U;
		return;
	}

	_registers[10] = deviceInvalid;
	if (operation == 13U) {
		if (!rangeValid(address, sizeof(lexxos::ui::UiCapabilities))) {
			return;
		}

		auto const caps = lexxos::ui::UiService::capabilities(_uiSession);
		std::memcpy(_memory + address, &caps, sizeof(caps));
		_registers[10] = 0U;
		return;
	}

	if (operation == 15U) {
		if (bytes < sizeof(lexxos::ui::TreePatchHeader) || !rangeValid(address, sizeof(lexxos::ui::TreePatchHeader))) {
			return;
		}

		lexxos::ui::TreePatchHeader header{};
		std::memcpy(&header, _memory + address, sizeof(header));
		if (header.version != 1U || header.count == 0U || header.count > lexxos::ui::maxTreePatchOperations || bytes != sizeof(header) + static_cast<std::uint32_t>(header.count) * sizeof(lexxos::ui::TreePatchOperation) || !rangeValid(address, bytes)) {
			return;
		}

		auto* copied = new (std::nothrow) lexxos::ui::TreePatchOperation[header.count];
		_registers[10] = deviceNotReady;
		if (copied == nullptr) {
			return;
		}

		std::memcpy(copied, _memory + address + sizeof(header),
		    header.count * sizeof(lexxos::ui::TreePatchOperation));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::patchTreeV3(
		    _context, _uiSession, header, copied));
		delete[] copied;
		return;
	}

	if (operation == 16U) {
		lexxos::ui::TreeUpdateHeader header{};
		_registers[10] = deviceInvalid;
		if (bytes < sizeof(header) || !rangeValid(address, sizeof(header))) {
			return;
		}

		std::memcpy(&header, _memory + address, sizeof(header));
		if (header.version != 1U || header.count == 0U || header.count > _uiSession.budget().maxControls || bytes != sizeof(header) + static_cast<std::uint32_t>(header.count) * sizeof(lexxos::ui::Control) || !rangeValid(address, bytes)) {
			return;
		}

		std::unique_ptr<lexxos::ui::Control[]> controls{new (std::nothrow) lexxos::ui::Control[header.count]};
		_registers[10] = deviceNotReady;
		if (!controls) {
			return;
		}

		std::memcpy(controls.get(), _memory + address + sizeof(header),
		    header.count * sizeof(lexxos::ui::Control));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::updateTreeBatch(
		    _context, _uiSession, header, controls.get()));
		return;
	}

	if (operation == 14U) {
		if (bytes < sizeof(lexxos::ui::TreeHeaderV3) || !rangeValid(address, sizeof(lexxos::ui::TreeHeaderV3))) {
			return;
		}

		lexxos::ui::TreeHeaderV3 header{};
		std::memcpy(&header, _memory + address, sizeof(header));
		if (header.version != 3U || header.count > _uiSession.budget().maxNodes || bytes != sizeof(header) + static_cast<std::uint32_t>(header.count) * sizeof(lexxos::ui::TreeNode) || !rangeValid(address, bytes)) {
			return;
		}

		auto* nodes = new (std::nothrow) lexxos::ui::TreeNode[header.count];
		_registers[10] = deviceNotReady;
		if (nodes == nullptr) {
			return;
		}

		std::memcpy(nodes, _memory + address + sizeof(header),
		    header.count * sizeof(lexxos::ui::TreeNode));
		int const status = lexxos::ui::UiService::showTreeV3(_context, _uiSession, header, nodes);
		delete[] nodes;
		_registers[10] = static_cast<std::uint32_t>(status);
		if (status == 0) {
			_graphicsEnabled = true;
			_context.programInput().reset(_guestInputCursor);
		}

		return;
	}

	if (operation == 6U) {
		if (!rangeValid(address, sizeof(lexxos::ui::Document))) {
			return;
		}

		lexxos::ui::Document document{};
		std::memcpy(&document, _memory + address, sizeof(document));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::show(_context, _uiSession, document));
		if (_registers[10] == 0U) {
			_graphicsEnabled = true;
			_context.programInput().reset(_guestInputCursor);
		}

		return;
	}

	if (operation == 12U) { // v2: complete validated widget tree.
		if (!rangeValid(address, sizeof(lexxos::ui::TreeDocument))) {
			return;
		}

		lexxos::ui::TreeDocument document{};
		std::memcpy(&document, _memory + address, sizeof(document));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::showTree(_context, _uiSession, document));
		if (_registers[10] == 0U) {
			_graphicsEnabled = true;
			_context.programInput().reset(_guestInputCursor);
		}

		return;
	}

	if (operation == 11U) {
		if (!rangeValid(address, sizeof(lexxos::ui::DialogDescriptor))) {
			return;
		}

		lexxos::ui::DialogDescriptor dialog{};
		std::memcpy(&dialog, _memory + address, sizeof(dialog));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::showDialog(_context, _uiSession, dialog));
		return;
	}

	if (operation == 9U) {
		if (!rangeValid(address, sizeof(lexxos::ui::Control))) {
			return;
		}

		lexxos::ui::Control control{};
		std::memcpy(&control, _memory + address, sizeof(control));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::update(_context, _uiSession, control));
		return;
	}

	if (operation == 10U) {
		if (!rangeValid(address, sizeof(lexxos::ui::ListItems))) {
			return;
		}

		lexxos::ui::ListItems items{};
		std::memcpy(&items, _memory + address, sizeof(items));
		_registers[10] = static_cast<std::uint32_t>(lexxos::ui::UiService::setItems(_context, _uiSession, items));
		return;
	}

	if (operation != 7U || !rangeValid(address, sizeof(lexxos::ui::Event))) {
		return;
	}

	_registers[10] = deviceDenied;
	if (!hasCapability(LexxCapabilityKeyboard)) {
		return;
	}

	lexxos::ui::Event event{};
	bool cancelled = false;
	int const result = lexxos::ui::UiService::poll(_context, _uiSession, _guestInputCursor, event, cancelled);
	if (cancelled) {
		_status = ExternalProgramStatus::Cancelled;
		_running = false;
		_registers[10] = lexxSyscallCancelled;
		return;
	}

	if (result >= 0) {
		std::memcpy(_memory + address, &event, sizeof(event));
	}

	_registers[10] = static_cast<std::uint32_t>(result);
}

void Rv32iVirtualMachine::handleGraphicsSyscall() {
	auto const operation = _registers[10];
	auto const address = _registers[11];
	auto const count = _registers[12];
	_registers[10] = deviceDenied;
	if (!hasCapability(LexxCapabilityGraphics)) {
		return;
	}

	if (operation >= 6U && operation <= 16U) {
		handleSystemUi(operation, address, count);
		return;
	}

	if (operation == 1U && address == 0U) {
		releaseGraphics();
		_registers[10] = 0U;
		return;
	}

	if (operation == 17U) {
		_registers[10] = deviceInvalid;
		if (!rangeValid(address, 20U)) {
			return;
		}

		constexpr auto policy = runtime::standardRuntimeProfile.graphics;
		writeLe32(_memory + address, static_cast<std::uint32_t>(policy.maxImageHandles));
		writeLe32(_memory + address + 4U, policy.maxImageDimension);
		writeLe32(_memory + address + 8U, policy.maxImagePixels);
		writeLe32(_memory + address + 12U, static_cast<std::uint32_t>(policy.imageCacheBytes));
		writeLe32(_memory + address + 16U, static_cast<std::uint32_t>(policy.streamChunkBytes));
		_registers[10] = 0U;
		return;
	}

	if (operation == 4U) {
		unloadGraphicsImage(address);
		return;
	}

	auto* display = _context.devices().primaryDisplayDevice();
	_registers[10] = deviceNotFound;
	if (display == nullptr) {
		return;
	}

	_registers[10] = deviceNotReady;
	if (!display->isReady()) {
		return;
	}

	if (operation == 5U) { // Present completion barrier for reliable timings.
		_registers[10] = deviceBusy;
		if (!_graphicsEnabled || !_context.hasProgramFocus() || !lexxos::app::guest_graphics_mode::owns(_context.processId()) || lexxos::app::console_mirror::mode() != lexxos::app::ConsoleMirrorMode::Graphics) {
			return;
		}

		auto lock = _context.devices().lockDisplayDevice(display->name(),
		    lexxos::devices::DeviceResourcePriority::Interactive, "lexx-graphics");
		if (!lock) {
			return;
		}

		_registers[10] = display->waitIdle() ? 0U : deviceIoError;
		return;
	}

	if (operation == 0U) {
		_registers[10] = deviceInvalid;
		if (!rangeValid(address, 8U)) {
			return;
		}

		auto const width = display->width();
		auto const height = display->height();
		if (width <= 0 || width > 65535 || height <= 0 || height > 65535) {
			_registers[10] = deviceUnsupported;
			return;
		}

		writeLe16(_memory + address, static_cast<std::uint16_t>(width));
		writeLe16(_memory + address + 2U, static_cast<std::uint16_t>(height));
		writeLe16(_memory + address + 4U,
		    display->nativeFormat() == lexxos::devices::PixelFormat::Rgb565 ? 1U : 0U);
		writeLe16(_memory + address + 6U, 0U);
		_registers[10] = 0U;
		return;
	}

	_registers[10] = deviceInvalid;

	if (operation == 3U) {
		if (count > 1U) {
			return;
		}

		loadGraphicsImage(address, count == 1U);
		return;
	}

	if (operation == 1U) {
		if (address != 1U || !_context.hasProgramFocus()) {
			return;
		}

		if (!lexxos::app::guest_graphics_mode::acquire(_context.processId())) {
			_registers[10] = deviceBusy;
			return;
		}

		_graphicsEnabled = true;
		lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Graphics);
		_registers[10] = 0U;
		return;
	}

	if (operation != 2U || count == 0U || count > 64U || !rangeValid(address, count * 24U)) {
		return;
	}

	_registers[10] = deviceBusy;
	if (!_graphicsEnabled || !_context.hasProgramFocus() || !lexxos::app::guest_graphics_mode::owns(_context.processId()) || lexxos::app::console_mirror::mode() != lexxos::app::ConsoleMirrorMode::Graphics) {
		return;
	}

	_registers[10] = deviceUnsupported;
	if (display->nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		return;
	}

	// Validate every command before touching the display.
	for (std::uint32_t i = 0; i < count; ++i) {
		auto const* cmd = _memory + address + i * 24U;
		auto const type = readLe32(cmd);
		if (type == 2U) {
			continue;
		}

		auto const x = static_cast<std::int32_t>(readLe32(cmd + 4U));
		auto const y = static_cast<std::int32_t>(readLe32(cmd + 8U));
		auto const w = static_cast<std::int32_t>(readLe32(cmd + 12U));
		auto const h = static_cast<std::int32_t>(readLe32(cmd + 16U));
		bool valid = false;

		if (type == 1U || type == 5U) { // Filled/outlined rectangle
			valid = graphicsRectValid(*display, x, y, w, h);
		} else if (type == 3U || type == 7U || (type >= 9U && type <= 13U)) { // Image / scaled image / atlas region
			valid = x >= 0 && y >= 0 && x < display->width() && y < display->height() && w > 0 && static_cast<std::size_t>(w) <= _graphicsImages.size() && (_graphicsImages[w - 1U].pixels != nullptr || _graphicsImages[w - 1U].streamed);
			if (valid && type == 9U) {
				valid = h == 1 || h == 2 || h == 4 || h == 8 || h == 16;
			}

			if (valid && (type == 7U || (type >= 10U && type <= 13U))) {
				auto const& image = _graphicsImages[w - 1U];
				auto const packed = static_cast<std::uint32_t>(h);
				auto const color = readLe32(cmd + 20U);
				std::uint32_t const sw = packed & 0xFFFFU, sh = packed >> 16U;
				std::uint32_t const sx = color & 0xFFFFU, sy = color >> 16U;
				valid = sw != 0U && sh != 0U && sx < image.width && sy < image.height && sw <= image.width - sx && sh <= image.height - sy;
			}
		} else if (type == 4U || type == 8U) { // Opaque / transparent text
			std::uint32_t length = 0;
			valid = x >= 0 && y >= 0 && x < display->width() && (h == 1 || h == 2) && 7 * h <= display->height() - y && graphicsTextValid(static_cast<std::uint32_t>(w), length);
		} else if (type == 6U) { // Pixel
			valid = graphicsRectValid(*display, x, y, 1, 1);
		}

		if (!valid) {
			// Report only the first invalid command per VM to avoid flooding the
			// console during the application's render loop. All values are
			// hexadecimal; this is diagnostic output, not a guest ABI change.
			if (!_graphicsInvalidReported) {
				_graphicsInvalidReported = true;
				_context.write("lexx-graphics: invalid command index=0x");
				writeHex32(_context, i);
				_context.write(" type=0x");
				writeHex32(_context, type);
				_context.write(" x=0x");
				writeHex32(_context, static_cast<std::uint32_t>(x));
				_context.write(" y=0x");
				writeHex32(_context, static_cast<std::uint32_t>(y));
				_context.write(" width=0x");
				writeHex32(_context, static_cast<std::uint32_t>(w));
				_context.write(" height=0x");
				writeHex32(_context, static_cast<std::uint32_t>(h));
				_context.writeLine("");
			}

			_registers[10] = deviceInvalid;
			return;
		}
	}

	ScopedGraphicsTiming submitTimer{_context, _graphicsSlowLast[5],
	    _graphicsSlowReported[5], "frame.submit", 120U};
	auto lock = _context.devices().lockDisplayDevice(display->name(),
	    lexxos::devices::DeviceResourcePriority::Interactive, "lexx-graphics");
	_registers[10] = deviceBusy;
	if (!lock) {
		return;
	}

	_registers[10] = deviceIoError;
	bool began = false;
	{
		ScopedGraphicsTiming beginTimer{_context, _graphicsSlowLast[6],
		    _graphicsSlowReported[6], "frame.begin", 60U};
		began = display->beginFrame();
	}
	if (!began) {
		return;
	}

	bool ok = true;
	for (std::uint32_t i = 0; i < count && ok; ++i) {
		auto const* cmd = _memory + address + i * 24U;
		auto const type = readLe32(cmd);
		int const x = static_cast<std::int32_t>(readLe32(cmd + 4U));
		int const y = static_cast<std::int32_t>(readLe32(cmd + 8U));
		int const w = static_cast<std::int32_t>(readLe32(cmd + 12U));
		int const h = static_cast<std::int32_t>(readLe32(cmd + 16U));
		std::uint32_t const color = readLe32(cmd + 20U);

		if (type >= 9U && type <= 13U) {
			// Scale in native OS memory; the guest provides only a command.
			auto const& image = _graphicsImages[w - 1U];
			ScopedGraphicsTiming scaledTimer{_context,
			    _graphicsSlowLast[image.streamed ? 3U : 2U],
			    _graphicsSlowReported[image.streamed ? 3U : 2U],
			    image.streamed ? "sprite.scaled.stream" : "sprite.scaled.cached", 80U};
			int const scale = type == 9U ? h : (1 << (type - 9U));
			bool const region = type != 9U;
			std::uint32_t const packed = static_cast<std::uint32_t>(h);
			int const sx = region ? static_cast<int>(color & 0xFFFFU) : 0;
			int const sy = region ? static_cast<int>(color >> 16U) : 0;
			int const sw = region ? static_cast<int>(packed & 0xFFFFU) : image.width;
			int const sh = region ? static_cast<int>(packed >> 16U) : image.height;
			int const cw = std::min(sw * scale, display->width() - x);
			int const ch = std::min(sh * scale, display->height() - y);
			constexpr int chunkPixels = 128;
			std::array<std::uint16_t, chunkPixels> sourceRgb{};
			std::array<std::uint8_t, chunkPixels> sourceAlpha{};
			std::array<std::uint8_t, chunkPixels * 2> sourceBytes{};
			std::array<std::uint16_t, chunkPixels> scaledRgb{};
			std::array<std::uint8_t, chunkPixels> scaledAlpha{};

			lexxos::vfs::FileHandle file{};
			if (image.streamed) {
				ok = _context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, file)
				    == lexxos::vfs::OpenStatus::Ok;
			}

			for (int row = 0; row < ch && ok; row += scale) {
				int const sourceRow = sy + row / scale;
				int const rows = std::min(scale, ch - row);
				for (int column = 0; column < cw && ok; column += chunkPixels) {
					int const pixels = std::min(chunkPixels, cw - column);
					int const first = column / scale;
					int const sourceCount = (column + pixels - 1) / scale - first + 1;
					std::uint32_t const offset = static_cast<std::uint32_t>(sourceRow) * image.width + sx + first;

					if (image.streamed) {
						ok = file.seek(static_cast<std::uint64_t>(image.resourceOffset) + offset * 2U)
						    && resources::readExact(file, sourceBytes.data(), sourceCount * 2U);
						if (ok && image.hasAlpha) {
							std::uint32_t const allPixels = static_cast<std::uint32_t>(image.width) * image.height;
							ok = file.seek(static_cast<std::uint64_t>(image.resourceOffset) + allPixels * 2U + offset)
							    && resources::readExact(file, sourceAlpha.data(), sourceCount);
						}

						for (int p = 0; p < sourceCount && ok; ++p) {
							sourceRgb[p] = graphics_asset::read16(sourceBytes.data() + p * 2);
						}
					} else {
						for (int p = 0; p < sourceCount; ++p) {
							sourceRgb[p] = image.pixels[offset + p];
							if (image.hasAlpha) {
								sourceAlpha[p] = image.alpha[offset + p];
							}
						}
					}

					for (int p = 0; p < pixels && ok; ++p) {
						int const source = (column + p) / scale - first;
						scaledRgb[p] = sourceRgb[source];
						if (image.hasAlpha) {
							scaledAlpha[p] = sourceAlpha[source];
						}
					}

					for (int repeat = 0; repeat < rows && ok; ++repeat) {
						lexxos::devices::Rect const rect{x + column, y + row + repeat, pixels, 1};
						ok = image.hasAlpha
						    ? display->blitRgb565Alpha(rect, scaledRgb.data(), scaledAlpha.data(), pixels)
						    : display->blitRgb565(rect, scaledRgb.data(), pixels);
					}
				}
			}
		} else if (type == 3U || type == 7U) {
			auto const& image = _graphicsImages[w - 1U];
			std::uint32_t const packed = static_cast<std::uint32_t>(h);
			int const sx = type == 7U ? static_cast<int>(color & 0xFFFFU) : 0;
			int const sy = type == 7U ? static_cast<int>(color >> 16U) : 0;
			int const sw = type == 7U ? static_cast<int>(packed & 0xFFFFU) : image.width;
			int const sh = type == 7U ? static_cast<int>(packed >> 16U) : image.height;
			int const cw = std::min(sw, display->width() - x);
			int const ch = std::min(sh, display->height() - y);
			std::size_t const offset = static_cast<std::size_t>(sy) * image.width + sx;
			lexxos::devices::Rect const rect{x, y, cw, ch};
			ok = image.streamed
			    ? drawStreamedGraphicsImage(*display, image, x, y, sx, sy, cw, ch)
			    : image.alpha != nullptr
			    ? display->blitRgb565Alpha(rect, image.pixels + offset, image.alpha + offset, image.width)
			    : display->blitRgb565(rect, image.pixels + offset, image.width);
		} else if (type == 4U || type == 8U) {
			ok = drawGraphicsText(*display, x, y, static_cast<std::uint32_t>(w), h,
			    color, type == 8U);
		} else if (type == 5U) {
			std::uint16_t const rgb = static_cast<std::uint16_t>(color);
			ok = display->fillRgb565({x, y, w, 1}, rgb)
			    && (h == 1 || display->fillRgb565({x, y + h - 1, w, 1}, rgb))
			    && (h <= 2 || display->fillRgb565({x, y + 1, 1, h - 2}, rgb))
			    && (h <= 2 || w == 1 || display->fillRgb565({x + w - 1, y + 1, 1, h - 2}, rgb));
		} else {
			lexxos::devices::Rect const rect = type == 2U
			    ? lexxos::devices::Rect{0, 0, display->width(), display->height()}
			    : type == 6U ? lexxos::devices::Rect{x, y, 1, 1}
			                 : lexxos::devices::Rect{x, y, w, h};
			ok = display->fillRgb565(rect, static_cast<std::uint16_t>(color));
		}
	}

	bool published = false;
	{
		ScopedGraphicsTiming endTimer{_context, _graphicsSlowLast[7],
		    _graphicsSlowReported[7], "frame.present", 80U};
		published = display->endFrame();
	}
	_registers[10] = ok && published ? 0U : deviceIoError;
}

} // namespace lexxos::external_program::detail
