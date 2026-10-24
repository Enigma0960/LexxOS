#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include <lexxos/app/console_output.h>
#include <lexxos/apps/fs/move_program.h>
#include <lexxos/apps/fs/rename_program.h>
#include <lexxos/apps/storage/block_devices_program.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/gameboy/cartridge.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/shell/shell.h>
#include <lexxos/vfs/vfs.h>

namespace {

constexpr std::uint32_t fakeFlashOffset = 0x1000;
constexpr std::size_t fakeFlashSize = 32U * 1024U;
constexpr std::size_t fakeFlashEraseSize = 4096U;

std::string g_console;
std::uint32_t g_uptimeMs = 100;
std::uint32_t g_sleepCalls = 0;
std::uint32_t g_sleptMs = 0;
std::uint32_t g_flashProgramCalls = 0;
std::array<std::uint8_t, fakeFlashSize> g_fakeFlash{};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "platform services contract test failed: " << message << '\n';
		std::exit(1);
	}
}

void checkRenameInheritsMoveRuntime() {
	auto const move = lexxos::apps::filesystem_programs::MoveProgram::descriptor();
	auto const rename = lexxos::apps::filesystem_programs::RenameProgram::descriptor();
	auto const& a = move.runtime;
	auto const& b = rename.runtime;
	expect(rename.handler == move.handler, "rename uses mv handler");
	expect(rename.arguments.data == move.arguments.data && rename.arguments.size == move.arguments.size,
	    "rename uses mv arguments");
	expect(b.launchMode == a.launchMode && b.launchMode == lexxos::program::ProgramLaunchMode::ForegroundTask,
	    "rename remains a foreground task");
	expect(b.stackSizeBytes == a.stackSizeBytes && b.priority == a.priority,
	    "rename uses mv task settings");
	expect(b.memoryClass == a.memoryClass
	        && b.externalMemoryQuotaBytes == a.externalMemoryQuotaBytes
	        && b.internalMemoryQuotaBytes == a.internalMemoryQuotaBytes
	        && b.internalMemoryQuotaBytes >= 2U * lexxos::vfs::maxPathLength,
	    "rename inherits mv workspace quota");
	expect(b.cancellable == a.cancellable && b.allowBackground == a.allowBackground && b.interfaceKind == a.interfaceKind && b.coreAffinity == a.coreAffinity,
	    "rename inherits remaining mv runtime settings");
}

void resetFakeState() {
	g_console.clear();
	g_uptimeMs = 100;
	g_sleepCalls = 0;
	g_sleptMs = 0;
	g_flashProgramCalls = 0;
	g_fakeFlash.fill(0xFF);
}

void writeConsole(std::string_view text) {
	g_console.append(text.data(), text.size());
}

bool readConsoleChar(char&) {
	return false;
}

void sleepMs(std::uint32_t milliseconds) {
	++g_sleepCalls;
	g_sleptMs += milliseconds;
	g_uptimeMs += milliseconds;
}

std::uint32_t uptimeMs() {
	return g_uptimeMs;
}

lexxos::platform::BoardDescriptor const& fakeBoard() {
	static constexpr lexxos::platform::BoardDescriptor board{
	    .boardId = "contract-dev-board",
	    .boardName = "Contract Dev Board",
	    .vendor = "LexxOS tests",
	    .processorFamily = lexxos::platform::ProcessorFamily::NativeHost,
	    .processorName = "fake-host",
	};
	return board;
}

bool findFlashRegion(std::string_view name, lexxos::platform::GameBoyRomFlashRegion& output) {
	if (name != "gameboy-rom-cache") {
		output = {};
		return false;
	}

	output = {
	    .available = true,
	    .offset = fakeFlashOffset,
	    .capacity = static_cast<std::uint32_t>(g_fakeFlash.size()),
	    .eraseSize = fakeFlashEraseSize,
	    .programSize = 256,
	    .xipBase = g_fakeFlash.data(),
	};
	return true;
}

bool eraseAndProgramFlash(std::uint32_t offset, void const* data, std::size_t size) {
	if (offset < fakeFlashOffset || size > g_fakeFlash.size() || offset - fakeFlashOffset > g_fakeFlash.size() - size) {
		return false;
	}

	auto const localOffset = static_cast<std::size_t>(offset - fakeFlashOffset);
	std::memcpy(g_fakeFlash.data() + localOffset, data, size);
	++g_flashProgramCalls;
	return true;
}

void reboot() {
}

lexxos::platform::PlatformServices const& fakeServices() {
	static constexpr lexxos::platform::PlatformServices services{
	    .board = {
	              .current = fakeBoard,
	              },
	    .console = {
	              .write = writeConsole,
	              .readChar = readConsoleChar,
	              },
	    .clock = {
	              .sleepMs = sleepMs,
	              .uptimeMs = uptimeMs,
	              },
	    .flashRegions = {
	              .findRegion = findFlashRegion,
	              .eraseAndProgram = eraseAndProgramFlash,
	              },
	};
	return services;
}

lexxos::platform::BoardDescriptor const& capabilityBoard() {
	static constexpr lexxos::platform::BoardDescriptor board{
	    .boardId = "capability-dev-board",
	    .boardName = "Capability Dev Board",
	    .vendor = "LexxOS tests",
	    .processorFamily = lexxos::platform::ProcessorFamily::NativeHost,
	    .processorName = "fake-host",
	    .userGpio = {
	                 .pins = {{.pin = {12}, .label = "gpio0"}},
	                 .count = 1,
	                 },
	    .sdInterface = lexxos::platform::SdInterface::Spi,
	    .sdSpi = {
	                 .index = 0,
	                 .sck = {1},
	                 .mosi = {2},
	                 .miso = {3},
	                 .cs = {4},
	                 },
	    .psram = {
	                 .spi = {
	            .index = 1,
	            .sck = {5},
	            .mosi = {6},
	            .miso = {7},
	            .cs = {8},
	        },
	                 .sio2 = {9},
	                 .sio3 = {10},
	                 },
	    .audioBeep = {
	                 .enabled = true,
	                 .left = {11},
	                 },
	    .audioCodec = {
	                 .enabled = true,
	                 .codec = lexxos::platform::AudioCodecKind::Es8311,
	                 .i2sIndex = 0,
	                 .bclk = {12},
	                 .wordSelect = {13},
	                 .dataOut = {14},
	                 .i2cAddress = 0x18,
	                 .sampleRateHz = 48000,
	                 },
	    .wireless = {
	                 .wifi = true,
	                 .bluetooth = true,
	                 .radio = "fake-radio",
	                 },
	};
	return board;
}

lexxos::platform::PlatformServices const& capabilityServices() {
	static constexpr lexxos::platform::PlatformServices services{
	    .board = {
	              .current = capabilityBoard,
	              },
	    .console = {
	              .write = writeConsole,
	              .readChar = readConsoleChar,
	              },
	    .clock = {
	              .sleepMs = sleepMs,
	              .uptimeMs = uptimeMs,
	              },
	    .power = {
	              .reboot = reboot,
	              },
	    .flashRegions = {
	              .findRegion = findFlashRegion,
	              .eraseAndProgram = eraseAndProgramFlash,
	              },
	};
	return services;
}

class FakeBlockDevice final : public lexxos::devices::BlockDevice {
public:
	bool initialize() override {
		_ready = true;
		return true;
	}

	[[nodiscard]] bool isReady() const override {
		return _ready;
	}

	[[nodiscard]] std::uint32_t blockSizeBytes() const override {
		return blockSize;
	}

	[[nodiscard]] std::uint64_t blockCount() const override {
		return blockCountValue;
	}

	bool readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blocks) override {
		if (!_ready || buffer == nullptr || firstBlock + blocks > blockCountValue) {
			return false;
		}
		std::memset(buffer, 0xA5, blocks * blockSize);
		return true;
	}

	bool writeBlocks(std::uint64_t firstBlock, void const*, std::size_t blocks) override {
		return _ready && firstBlock + blocks <= blockCountValue;
	}

	static constexpr std::uint32_t blockSize = 512;
	static constexpr std::uint64_t blockCountValue = 16;

private:
	bool _ready{true};
};

class FakeDisplayDevice final : public lexxos::devices::DisplayDevice {
public:
	[[nodiscard]] std::string_view name() const override {
		return "fake-display";
	}

	[[nodiscard]] lexxos::devices::DeviceKind kind() const override {
		return lexxos::devices::DeviceKind::Display;
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] int width() const override {
		return 320;
	}

	[[nodiscard]] int height() const override {
		return 240;
	}

	[[nodiscard]] lexxos::devices::PixelFormat nativeFormat() const override {
		return lexxos::devices::PixelFormat::Rgb565;
	}

	[[nodiscard]] bool supportsPowerMode() const override {
		return true;
	}

	[[nodiscard]] lexxos::devices::DisplayPowerMode powerMode() const override {
		return lexxos::devices::DisplayPowerMode::On;
	}

	bool setPowerMode(lexxos::devices::DisplayPowerMode) override {
		return true;
	}

	bool clear(std::uint16_t) override {
		return true;
	}

	bool beginFrame() override {
		return true;
	}

	bool blitRgb565(lexxos::devices::Rect const&, std::uint16_t const*, int) override {
		return true;
	}

	bool endFrame() override {
		return true;
	}

	bool beginRgb565Stream(lexxos::devices::Rect const&) override {
		return true;
	}

	bool writeRgb565Stream(std::uint16_t const*, int) override {
		return true;
	}

	bool writeRgb565BytesStream(std::uint8_t const*, int) override {
		return true;
	}

	bool endRgb565Stream() override {
		return true;
	}

	bool configureVerticalScroll(int, int, int) override {
		return false;
	}

	bool setVerticalScrollStart(int) override {
		return false;
	}
};

class FakeInputDevice final : public lexxos::devices::InputDevice {
public:
	[[nodiscard]] std::string_view name() const override {
		return "fake-keyboard";
	}

	[[nodiscard]] lexxos::devices::DeviceKind kind() const override {
		return lexxos::devices::DeviceKind::Input;
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	void update() override {
	}

	void setRawEventReporting(bool) override {
	}

	bool pollEvent(lexxos::devices::InputEvent&) override {
		return false;
	}

	[[nodiscard]] bool isPressed(lexxos::devices::KeyCode) const override {
		return false;
	}

	[[nodiscard]] std::uint32_t rawState() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t rawKeyCount() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t rawRowCount() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t rawColumnCount() const override {
		return 0;
	}
};

class FakeGamepadDevice final : public lexxos::devices::GamepadDevice {
public:
	[[nodiscard]] std::string_view name() const override {
		return "fake-gamepad";
	}

	[[nodiscard]] lexxos::devices::DeviceKind kind() const override {
		return lexxos::devices::DeviceKind::Gamepad;
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] lexxos::devices::GamepadState state() override {
		return {};
	}
};

class FakePointingDevice final : public lexxos::devices::PointingDevice {
public:
	[[nodiscard]] std::string_view name() const override {
		return "fake-pointer";
	}

	[[nodiscard]] lexxos::devices::DeviceKind kind() const override {
		return lexxos::devices::DeviceKind::Pointing;
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] lexxos::devices::PointingDeviceState state() const override {
		return {};
	}

	void resetPosition() override {
	}
};

class FakePicoCalcSouthbridgeService final : public lexxos::devices::PicoCalcSouthbridgeRuntimeService {
public:
	[[nodiscard]] std::string_view name() const override {
		return "picocalc-southbridge";
	}

	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] bool southbridgeSnapshot(lexxos::devices::PicoCalcSouthbridgeSnapshot&) const override {
		return false;
	}

	[[nodiscard]] std::uint32_t southbridgeSnapshotMs() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t southbridgeFullSnapshotMs() const override {
		return 0;
	}

	[[nodiscard]] bool southbridgeLastRefreshOk() const override {
		return false;
	}

	[[nodiscard]] std::uint32_t southbridgeSnapshotUpdates() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t southbridgeSnapshotFailures() const override {
		return 0;
	}

	[[nodiscard]] lexxos::devices::PicoCalcSouthbridgeCommandStatus southbridgeLastCommandStatus() const override {
		return {};
	}

	[[nodiscard]] bool refreshSouthbridgeSnapshot(std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool requestLcdBacklight(std::uint8_t, std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool requestKeyboardBacklight(std::uint8_t, std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool requestSouthbridgeReset(std::uint8_t, std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool requestPowerOff(std::uint32_t) override {
		return false;
	}

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	[[nodiscard]] bool readJcsFirmwareInfo(lexxos::devices::PicoCalcJcsFirmwareInfo&) const override {
		return false;
	}

	[[nodiscard]] bool readJcsRtcConfig(lexxos::devices::PicoCalcJcsRtcConfig&) const override {
		return false;
	}

	[[nodiscard]] bool writeJcsRtcConfig(lexxos::devices::PicoCalcJcsRtcConfig, std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool readJcsRtcDateTime(lexxos::devices::rtc::DateTime&) const override {
		return false;
	}

	[[nodiscard]] bool writeJcsRtcDateTime(lexxos::devices::rtc::DateTime const&, std::uint32_t) override {
		return false;
	}

	[[nodiscard]] bool readJcsRtcAlarm(lexxos::devices::rtc::DateTime&) const override {
		return false;
	}

	[[nodiscard]] bool writeJcsRtcAlarm(lexxos::devices::rtc::DateTime const&, std::uint32_t) override {
		return false;
	}
#endif

	[[nodiscard]] std::uint32_t rawState() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t rawKeyCount() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t staleScanCodeReleases() const override {
		return 0;
	}

	[[nodiscard]] std::uint32_t staleReleaseCount() const override {
		return 0;
	}
};

class StaticRomFilesystem final : public lexxos::vfs::ReadOnlyFilesystem {
public:
	explicit StaticRomFilesystem(std::uint8_t const* data, std::size_t size)
	    : _data(data)
	    , _size(size) {
	}

	[[nodiscard]] bool isReady() const override {
		return _data != nullptr;
	}

	[[nodiscard]] std::string_view typeName() const override {
		return "static-rom";
	}

	bool stat(std::string_view path, lexxos::vfs::Stat& output) override {
		output = {};
		if (path == "/") {
			output.type = lexxos::vfs::NodeType::Directory;
			output.ready = true;
			return true;
		}
		if (path == "/fake.gb") {
			output.type = lexxos::vfs::NodeType::RegularFile;
			output.sizeBytes = _size;
			output.ready = true;
			return true;
		}
		return false;
	}

	bool list(std::string_view path, lexxos::vfs::DirectoryVisitor visitor, void* userData) override {
		if (path != "/") {
			return false;
		}
		return visitor(
		    {
		        .name = "fake.gb",
		        .type = lexxos::vfs::NodeType::RegularFile,
		        .sizeBytes = _size,
		    },
		    userData);
	}

	lexxos::vfs::ReadStatus read(
	    std::string_view path,
	    std::uint64_t offset,
	    void* buffer,
	    std::size_t capacity,
	    std::size_t& bytesRead) override {
		bytesRead = 0;
		if (path != "/fake.gb") {
			return lexxos::vfs::ReadStatus::NotFound;
		}
		if (buffer == nullptr) {
			return lexxos::vfs::ReadStatus::InvalidPath;
		}
		if (offset >= _size || capacity == 0) {
			return lexxos::vfs::ReadStatus::Ok;
		}

		auto const available = _size - static_cast<std::size_t>(offset);
		bytesRead = std::min(available, capacity);
		std::memcpy(buffer, _data + offset, bytesRead);
		return lexxos::vfs::ReadStatus::Ok;
	}

	lexxos::vfs::ReadStatus readText(std::string_view, lexxos::vfs::TextWriter, void*) override {
		return lexxos::vfs::ReadStatus::NotReadable;
	}

private:
	std::uint8_t const* _data{nullptr};
	std::size_t _size{0};
};

class WritableShellConfigFilesystem final : public lexxos::vfs::ReadOnlyFilesystem {
public:
	[[nodiscard]] bool isReady() const override {
		return true;
	}

	[[nodiscard]] std::string_view typeName() const override {
		return "writable-config";
	}

	[[nodiscard]] bool isReadOnly() const override {
		return false;
	}

	bool stat(std::string_view path, lexxos::vfs::Stat& output) override {
		output = {};
		if (path == "/" || (path == "/.lexx" && _hasConfigDirectory)) {
			output.type = lexxos::vfs::NodeType::Directory;
			output.ready = true;
			return true;
		}
		if (path == "/.lexx/shell.conf" && _hasConfigFile) {
			output.type = lexxos::vfs::NodeType::RegularFile;
			output.sizeBytes = _config.size();
			output.ready = true;
			return true;
		}
		return false;
	}

	bool list(std::string_view path, lexxos::vfs::DirectoryVisitor visitor, void* userData) override {
		if (path == "/" && _hasConfigDirectory) {
			return visitor({.name = ".lexx", .type = lexxos::vfs::NodeType::Directory}, userData);
		}
		if (path == "/.lexx" && _hasConfigFile) {
			return visitor(
			    {
			        .name = "shell.conf",
			        .type = lexxos::vfs::NodeType::RegularFile,
			        .sizeBytes = _config.size(),
			    },
			    userData);
		}
		return path == "/" || path == "/.lexx";
	}

	lexxos::vfs::ReadStatus read(
	    std::string_view path,
	    std::uint64_t offset,
	    void* buffer,
	    std::size_t capacity,
	    std::size_t& bytesRead) override {
		bytesRead = 0;
		if (path != "/.lexx/shell.conf" || !_hasConfigFile) {
			return lexxos::vfs::ReadStatus::NotFound;
		}
		if (buffer == nullptr) {
			return lexxos::vfs::ReadStatus::InvalidPath;
		}
		if (offset >= _config.size() || capacity == 0) {
			return lexxos::vfs::ReadStatus::Ok;
		}
		auto const available = _config.size() - static_cast<std::size_t>(offset);
		bytesRead = std::min(available, capacity);
		std::memcpy(buffer, _config.data() + offset, bytesRead);
		return lexxos::vfs::ReadStatus::Ok;
	}

	lexxos::vfs::ReadStatus readText(std::string_view path, lexxos::vfs::TextWriter writer, void* userData) override {
		if (path != "/.lexx/shell.conf" || !_hasConfigFile) {
			return lexxos::vfs::ReadStatus::NotFound;
		}
		writer(_config, userData);
		return lexxos::vfs::ReadStatus::Ok;
	}

	lexxos::vfs::WriteStatus beginWrite(std::string_view path, lexxos::vfs::WriteMode mode) override {
		if (path != "/.lexx/shell.conf") {
			return lexxos::vfs::WriteStatus::InvalidPath;
		}
		if (!_hasConfigDirectory) {
			return lexxos::vfs::WriteStatus::NotFound;
		}
		_pending = mode == lexxos::vfs::WriteMode::Append && _hasConfigFile ? _config : std::string{};
		_writeOpen = true;
		return lexxos::vfs::WriteStatus::Ok;
	}

	lexxos::vfs::WriteStatus write(std::string_view path, std::uint64_t offset, std::string_view data) override {
		if (path != "/.lexx/shell.conf" || !_writeOpen) {
			return lexxos::vfs::WriteStatus::NotWritable;
		}
		if (offset > _pending.size()) {
			return lexxos::vfs::WriteStatus::InvalidPath;
		}
		auto const position = static_cast<std::size_t>(offset);
		if (position == _pending.size()) {
			_pending.append(data);
			return lexxos::vfs::WriteStatus::Ok;
		}
		if (position + data.size() > _pending.size()) {
			_pending.resize(position + data.size());
		}
		_pending.replace(position, data.size(), data);
		return lexxos::vfs::WriteStatus::Ok;
	}

	void closeWrite(std::string_view path) override {
		if (path != "/.lexx/shell.conf" || !_writeOpen) {
			return;
		}
		_config = _pending;
		_pending.clear();
		_hasConfigFile = true;
		_writeOpen = false;
	}

	lexxos::vfs::WriteStatus createDirectory(std::string_view path) override {
		if (path != "/.lexx") {
			return lexxos::vfs::WriteStatus::InvalidPath;
		}
		_hasConfigDirectory = true;
		return lexxos::vfs::WriteStatus::Ok;
	}

	[[nodiscard]] bool sync() override {
		return true;
	}

	[[nodiscard]] std::string_view config() const {
		return _config;
	}

private:
	std::string _config;
	std::string _pending;
	bool _hasConfigDirectory{false};
	bool _hasConfigFile{false};
	bool _writeOpen{false};
};

class FakeI2cBus final : public lexxos::devices::I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const&) override {
		_ready = true;
		return true;
	}

	[[nodiscard]] bool isReady() const override {
		return _ready;
	}

	bool probe(std::uint8_t) override {
		return false;
	}

	bool write(std::uint8_t, std::uint8_t const*, std::size_t) override {
		return false;
	}

	bool read(std::uint8_t, std::uint8_t*, std::size_t) override {
		return false;
	}

private:
	bool _ready{false};
};

void writeUnsignedImpl(lexxos::os::AppContext& context, std::uint64_t value) {
	char buffer[32]{};
	auto position = sizeof(buffer) - 1U;
	buffer[position] = '\0';
	if (value == 0) {
		context.write("0");
		return;
	}
	while (value != 0 && position > 0) {
		--position;
		buffer[position] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}
	context.write(&buffer[position]);
}

int fakeProgram(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	context.writeLine("fake-program-output");
	return 0;
}

bool registerFakeProgram(lexxos::os::OperatingSystem& os) {
	return os.programs().registerProgram({
	    .name = "fake",
	    .help = "fake shell program",
	    .handler = fakeProgram,
	    .allowExtraArguments = true,
	});
}

void feedLine(lexxos::shell::Shell& shell, std::string_view line) {
	for (auto const character : line) {
		shell.feed(character);
	}
	shell.feed('\n');
}

std::array<std::uint8_t, lexxos::gameboy::Cartridge::romBankSize * 2U> makeTestRom() {
	std::array<std::uint8_t, lexxos::gameboy::Cartridge::romBankSize * 2U> rom{};
	for (std::size_t index = 0; index < rom.size(); ++index) {
		rom[index] = static_cast<std::uint8_t>(index & 0xFFU);
	}

	static constexpr std::uint8_t logo[lexxos::gameboy::cartridgeLogoSize]{
	    0xCE,
	    0xED,
	    0x66,
	    0x66,
	    0xCC,
	    0x0D,
	    0x00,
	    0x0B,
	    0x03,
	    0x73,
	    0x00,
	    0x83,
	    0x00,
	    0x0C,
	    0x00,
	    0x0D,
	    0x00,
	    0x08,
	    0x11,
	    0x1F,
	    0x88,
	    0x89,
	    0x00,
	    0x0E,
	    0xDC,
	    0xCC,
	    0x6E,
	    0xE6,
	    0xDD,
	    0xDD,
	    0xD9,
	    0x99,
	    0xBB,
	    0xBB,
	    0x67,
	    0x63,
	    0x6E,
	    0x0E,
	    0xEC,
	    0xCC,
	    0xDD,
	    0xDC,
	    0x99,
	    0x9F,
	    0xBB,
	    0xB9,
	    0x33,
	    0x3E,
	};
	std::copy(std::begin(logo), std::end(logo), rom.begin() + lexxos::gameboy::cartridgeLogoOffset);

	constexpr std::string_view title{"FAKESVC"};
	std::copy(title.begin(), title.end(), rom.begin() + lexxos::gameboy::cartridgeTitleOffset);
	rom[lexxos::gameboy::cartridgeTypeOffset] = 0x00;
	rom[lexxos::gameboy::cartridgeRomSizeOffset] = 0x00;
	rom[lexxos::gameboy::cartridgeRamSizeOffset] = 0x00;
	rom[lexxos::gameboy::cartridgeDestinationOffset] = 0x01;
	rom[lexxos::gameboy::cartridgeOldLicenseeOffset] = 0x33;
	rom[lexxos::gameboy::cartridgeMaskRomVersionOffset] = 0x00;

	std::uint8_t checksum = 0;
	for (std::size_t offset = 0x134; offset <= 0x14CU; ++offset) {
		checksum = static_cast<std::uint8_t>(checksum - rom[offset] - 1U);
	}
	rom[lexxos::gameboy::cartridgeHeaderChecksumOffset] = checksum;
	return rom;
}

void checkShellUsesInjectedConsoleService() {
	resetFakeState();

	lexxos::os::OperatingSystem os{fakeServices()};
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);
	expect(registerFakeProgram(os), "register fake shell program");

	shell.printPrompt();
	feedLine(shell, "fake");

	expect(g_console.find("lexx:/> fake") != std::string::npos, "shell echoes input through fake console service");
	expect(g_console.find("fake-program-output") != std::string::npos, "shell program writes through fake console service");
}

void checkExternalConsoleOutputPreservesShellInputLine() {
	resetFakeState();

	lexxos::os::OperatingSystem os{fakeServices()};
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);

	shell.printPrompt();
	shell.feed('p');
	shell.feed('s');

	lexxos::app::console_output::write(fakeServices(), "wifi-autoconnect: connecting to ");
	lexxos::app::console_output::write(fakeServices(), "<SSID>");
	lexxos::app::console_output::write(fakeServices(), "\r\n");

	std::string const expected =
	    "lexx:/> ps\r\x1B[Kwifi-autoconnect: connecting to <SSID>\r\n\rlexx:/> ps\x1B[K";
	expect(g_console.find(expected) != std::string::npos, "external console output redraws active shell input line");
}

void checkPathCompletionKeepsLongFileNames() {
	resetFakeState();
	lexxos::vfs::initialize(fakeServices());

	constexpr std::string_view longName{"ExampleMidiFile_Finale_With_A_Long_Title.mid"};
	constexpr std::string_view longPath{"/tmp/ExampleMidiFile_Finale_With_A_Long_Title.mid"};
	expect(lexxos::vfs::writeText(longPath, "midi") == lexxos::vfs::WriteStatus::Ok, "create long tmpfs file name");

	lexxos::os::OperatingSystem os{fakeServices()};
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);

	shell.printPrompt();
	for (char const character : std::string_view{"mplayer /tmp/ExampleMidiFile_F"}) {
		shell.feed(character);
	}
	shell.feed('\t');

	expect(g_console.find(longName) != std::string::npos, "path completion should keep file names longer than 32 bytes");
	expect(g_console.find("mplayer /tmp/ExampleMidiFile_Finale_With_A_Long_Title.mid ") != std::string::npos,
	    "single long path completion should append a trailing space");
}

void checkShellAutosuggestUsesHistory() {
	resetFakeState();

	lexxos::os::OperatingSystem os{fakeServices()};
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);
	expect(registerFakeProgram(os), "register fake autosuggest program");

	shell.printPrompt();
	feedLine(shell, "fake --color red");
	g_console.clear();

	shell.feed('f');
	shell.feed('a');
	expect(g_console.find("\x1B[90mke --color red\x1B[0m") != std::string::npos,
	    "shell renders newest matching history suffix as dim ANSI text");

	shell.feed(0x06);
	expect(g_console.find("fake --color red\x1B[K") != std::string::npos, "Ctrl+F accepts autosuggestion suffix");

	shell.feed('\n');
	expect(g_console.find("fake-program-output") != std::string::npos, "accepted autosuggestion runs full command");
}

void checkShellConfigPersistsHistoryAndAutosuggest() {
	resetFakeState();

	lexxos::vfs::Vfs vfs;
	vfs.initialize(fakeServices());
	FakeBlockDevice block;
	WritableShellConfigFilesystem filesystem;
	expect(vfs.registerBlockDevice("sd0", &block), "register fake config block device");
	expect(vfs.mountFilesystem("sd0", "/mnt/sd", &filesystem) == lexxos::vfs::MountStatus::Ok,
	    "mount fake config filesystem");

	lexxos::os::OperatingSystem os{fakeServices(), vfs};
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);
	expect(registerFakeProgram(os), "register fake config program");

	shell.printPrompt();
	feedLine(shell, "fake persisted alpha");
	feedLine(shell, "fake persisted beta");
	shell.setAutosuggestEnabled(false);
	expect(shell.saveShellConfig(), "save shell config to fake SD");

	auto const saved = filesystem.config();
	expect(saved.find("autosuggest=off") != std::string_view::npos, "saved config keeps autosuggest flag");
	expect(saved.find("history=fake persisted alpha") != std::string_view::npos, "saved config keeps first history line");
	expect(saved.find("history=fake persisted beta") != std::string_view::npos, "saved config keeps second history line");

	lexxos::shell::Shell restored;
	restored.bindOperatingSystem(os);
	expect(restored.loadShellConfig(true), "load shell config from fake SD");
	expect(!restored.autosuggestEnabled(), "loaded config restores autosuggest flag");
	expect(restored.historySize() == 2, "loaded config restores history entries");

	g_console.clear();
	restored.printPrompt();
	restored.feed("fake persisted b");
	expect(g_console.find("\x1B[90m") == std::string::npos, "disabled autosuggest suppresses history suffix");

	restored.feed(0x03);
	restored.setAutosuggestEnabled(true);
	g_console.clear();
	restored.printPrompt();
	restored.feed("fake persisted b");
	expect(g_console.find("\x1B[90meta\x1B[0m") != std::string::npos,
	    "re-enabled autosuggest uses restored history");
}

void checkSensitiveShellHistory() {
	resetFakeState();
	lexxos::vfs::Vfs vfs;
	vfs.initialize(fakeServices());
	FakeBlockDevice block;
	WritableShellConfigFilesystem filesystem;
	expect(vfs.registerBlockDevice("sd0", &block), "register history test SD");
	expect(vfs.mountFilesystem("sd0", "/mnt/sd", &filesystem) == lexxos::vfs::MountStatus::Ok, "mount history SD");
	lexxos::os::OperatingSystem os{fakeServices(), vfs};
	expect(registerFakeProgram(os), "register safe command");
	static constexpr lexxos::program::ProgramArgumentSpec args[] = {
	    {.name = "password", .help = "test password", .kind = lexxos::program::ProgramArgumentKind::Positional, .sensitive = true},
	};
	expect(os.programs().registerProgram({
	           .name = "test-login", .help = "test", .arguments = {args, 1},
                       .handler = fakeProgram
    }),
	    "register sensitive command");
	lexxos::shell::Shell shell;
	shell.bindOperatingSystem(os);
	shell.printPrompt();
	feedLine(shell, "fake safe-entry");
	for (auto line : {"test-login FakeSecret", "test-login --invalid FakeSecret", "wifi-connect TestNet FakeSecret",
	         "wifi-config set TestNet FakeSecret", "\"wifi-connect\" TestNet FakeSecret", "wifi-connect 'FakeSecret"}) {
		feedLine(shell, line);
	}
	expect(shell.historySize() == 1, "sensitive and malformed commands never enter history");
	expect(shell.saveShellConfig(), "save sanitized history");
	expect(filesystem.config().find("FakeSecret") == std::string_view::npos, "secret absent from SD config");
	g_console.clear();
	shell.feed("\x1b[A");
	expect(g_console.find("safe-entry") != std::string::npos, "up recalls last safe command");
	expect(g_console.find("FakeSecret") == std::string::npos, "up never recalls secret");
	shell.feed(0x03);
	g_console.clear();
	shell.feed("test-l");
	expect(g_console.find("FakeSecret") == std::string::npos, "autosuggest never reveals secret");
	shell.feed(0x03);

	// Simulate SD history written by old firmware, including unavailable Wi-Fi commands.
	std::string_view legacy = "autosuggest=on\nhistory=fake safe-import\nhistory=wifi-connect TestNet OldSecret\nhistory=wifi-config set TestNet OldSecret\nhistory=test-login OldSecret\n";
	expect(filesystem.beginWrite("/.lexx/shell.conf", lexxos::vfs::WriteMode::CreateTruncate) == lexxos::vfs::WriteStatus::Ok,
	    "open legacy history");
	expect(filesystem.write("/.lexx/shell.conf", 0, legacy) == lexxos::vfs::WriteStatus::Ok, "seed legacy history");
	filesystem.closeWrite("/.lexx/shell.conf");
	lexxos::shell::Shell restored;
	restored.bindOperatingSystem(os);
	expect(restored.loadShellConfig(true), "load legacy history");
	expect(restored.historySize() == 1, "legacy credentials filtered on load");
	g_console.clear();
	restored.printPrompt();
	restored.feed("wifi-");
	expect(g_console.find("OldSecret") == std::string::npos, "loaded secret not suggested");
	restored.feed(0x03);
	expect(restored.saveShellConfig(), "rewrite sanitized legacy history");
	expect(filesystem.config().find("OldSecret") == std::string_view::npos, "legacy secrets removed on save");
}

void checkStorageProgramUsesFakeServicesAndDeviceManager() {
	resetFakeState();

	lexxos::os::OperatingSystem os{fakeServices()};
	FakeBlockDevice blockDevice;
	expect(os.devices().registerBlockDevice("sd0", "fake SD block device", blockDevice), "register fake block device");

	auto context = os.createAppContext({});
	char const* argv[] = {"block-devs"};
	lexxos::program::ParsedArguments arguments;
	auto const exitCode = lexxos::apps::storage_programs::BlockDevicesProgram::run(
	    context,
	    {
	        .argc = 1,
	        .argv = argv,
	    },
	    arguments);

	expect(exitCode == 0, "block-devs exits successfully");
	expect(g_console.find("sd0") != std::string::npos, "storage command writes fake block device name through fake console service");
	expect(g_console.find("ready") != std::string::npos, "storage command sees fake block device readiness");
	expect(g_console.find("512") != std::string::npos, "storage command prints fake block size");
	expect(g_console.find("16") != std::string::npos, "storage command prints fake block count");
}

void checkPicoCalcSouthbridgeServiceLockUsesDeviceManager() {
	lexxos::devices::DeviceManager devices;
	FakePicoCalcSouthbridgeService service;

	expect(devices.registerPicoCalcSouthbridgeService("picocalc-southbridge", "fake southbridge service", service),
	    "register fake PicoCalc southbridge service");
	expect(devices.picoCalcSouthbridgeServiceCount() == 1, "registered one PicoCalc southbridge service");
	expect(devices.picoCalcSouthbridgeService("picocalc-southbridge") == &service, "lookup returns registered service");
	expect(!devices.isPicoCalcSouthbridgeServiceLocked("picocalc-southbridge"), "service starts unlocked");

	{
		auto lock = devices.lockPicoCalcSouthbridgeService(
		    "picocalc-southbridge",
		    lexxos::devices::DeviceResourcePriority::Interactive,
		    "unit-test");
		expect(lock.valid(), "lock registered PicoCalc southbridge service");

		auto const status = devices.picoCalcSouthbridgeServiceStatusAt(0);
		expect(status.locked, "service status reports locked");
		expect(status.priority == lexxos::devices::DeviceResourcePriority::Interactive, "service status keeps priority");
		expect(status.owner == "unit-test", "service status keeps owner");

		auto secondLock = devices.lockPicoCalcSouthbridgeService(
		    "picocalc-southbridge",
		    lexxos::devices::DeviceResourcePriority::Interactive,
		    "second-owner");
		expect(!secondLock.valid(), "second PicoCalc southbridge service lock is refused");
	}

	expect(!devices.isPicoCalcSouthbridgeServiceLocked("picocalc-southbridge"), "service unlocks when RAII lock is destroyed");
}

void checkRuntimeCapabilitiesFollowPlatformAndDeviceRegistration() {
	resetFakeState();

	lexxos::os::OperatingSystem os{capabilityServices()};
	os.initialize();

	auto capabilities = os.capabilities();
	expect(capabilities.has(lexxos::runtime::Capability::StorageRemovable), "board SD wiring exposes removable storage capability");
	expect(capabilities.has(lexxos::runtime::Capability::AudioTone), "board audio wiring exposes audio tone capability");
	expect(capabilities.has(lexxos::runtime::Capability::AudioPcm), "board audio codec wiring exposes PCM audio capability");
	expect(capabilities.has(lexxos::runtime::Capability::WirelessWifi), "board wireless profile exposes Wi-Fi capability");
	expect(capabilities.has(lexxos::runtime::Capability::WirelessBluetooth), "board wireless profile exposes Bluetooth capability");
	expect(capabilities.has(lexxos::runtime::Capability::FlashNamedRegions), "platform flash service exposes named regions capability");
	expect(capabilities.has(lexxos::runtime::Capability::ExternalPsram), "board PSRAM wiring exposes external memory capability");
	expect(capabilities.has(lexxos::runtime::Capability::GpioUser), "board user GPIO exposes GPIO capability");
	expect(capabilities.has(lexxos::runtime::Capability::PowerReboot), "platform power service exposes reboot capability");
	expect(!capabilities.has(lexxos::runtime::Capability::StorageBlock), "block storage requires a registered device");
	expect(!capabilities.has(lexxos::runtime::Capability::DisplayRgb565), "RGB565 display requires a registered display");
	expect(!capabilities.has(lexxos::runtime::Capability::InputKeyboard), "keyboard input requires a registered input device");
	expect(!capabilities.has(lexxos::runtime::Capability::InputPointer), "pointer input requires a registered pointing device");
	expect(!capabilities.has(lexxos::runtime::Capability::InputGamepad), "gamepad input requires a registered gamepad");
	expect(!capabilities.has(lexxos::runtime::Capability::PicoCalcSouthbridge), "southbridge capability requires a registered service");

	FakeBlockDevice block;
	FakeDisplayDevice display;
	FakeInputDevice input;
	FakePointingDevice pointer;
	FakeGamepadDevice gamepad;
	FakePicoCalcSouthbridgeService southbridge;

	expect(os.devices().registerBlockDevice("sd0", "fake SD block device", block), "register capability block device");
	expect(os.devices().registerDisplayDevice(display.name(), "fake RGB565 display", display), "register capability display");
	expect(os.devices().registerInputDevice(input.name(), "fake keyboard input", input), "register capability input");
	expect(os.devices().registerPointingDevice(pointer.name(), "fake pointer", pointer), "register capability pointer");
	expect(os.devices().registerGamepadDevice(gamepad.name(), "fake gamepad", gamepad), "register capability gamepad");
	expect(os.devices().registerPicoCalcSouthbridgeService(southbridge.name(), "fake southbridge", southbridge),
	    "register capability southbridge");

	os.refreshCapabilities();
	capabilities = os.capabilities();
	expect(capabilities.has(lexxos::runtime::Capability::StorageBlock), "registered block device exposes block capability");
	expect(capabilities.has(lexxos::runtime::Capability::Display), "registered display exposes display capability");
	expect(capabilities.has(lexxos::runtime::Capability::DisplayRgb565), "registered RGB565 display exposes RGB565 capability");
	expect(capabilities.has(lexxos::runtime::Capability::DisplayPowerMode), "display power contract exposes power mode capability");
	expect(capabilities.has(lexxos::runtime::Capability::InputKeyboard), "registered input device exposes keyboard capability");
	expect(capabilities.has(lexxos::runtime::Capability::InputPointer), "registered pointing device exposes pointer capability");
	expect(capabilities.has(lexxos::runtime::Capability::InputGamepad), "registered gamepad exposes gamepad capability");
	expect(capabilities.has(lexxos::runtime::Capability::PicoCalcSouthbridge), "registered southbridge exposes service capability");

	auto context = os.createAppContext({});
	expect(context.capabilities().has(lexxos::runtime::Capability::DisplayRgb565), "AppContext exposes OS capabilities");
	expect(lexxos::runtime::capabilityName(lexxos::runtime::Capability::DisplayRgb565) == "display.rgb565",
	    "capability names are stable");
	expect(lexxos::runtime::capabilityName(lexxos::runtime::Capability::AudioPcm) == "audio.pcm",
	    "PCM audio capability name is stable");
}

void checkVfsInstancesKeepTmpfsIsolated() {
	lexxos::vfs::Vfs first;
	lexxos::vfs::Vfs second;
	first.initialize(fakeServices());
	second.initialize(fakeServices());

	expect(first.writeText("/tmp/shared", "first") == lexxos::vfs::WriteStatus::Ok, "write first VFS tmp file");

	lexxos::vfs::Stat stat{};
	expect(first.stat("/tmp/shared", stat), "first VFS sees its tmp file");
	expect(!second.stat("/tmp/shared", stat), "second VFS does not see first VFS tmp file");

	expect(second.writeText("/tmp/shared", "second") == lexxos::vfs::WriteStatus::Ok, "write second VFS tmp file");

	std::string firstText;
	std::string secondText;
	auto const writer = [](std::string_view text, void* userData) {
		auto& output = *static_cast<std::string*>(userData);
		output.append(text);
	};

	expect(first.readText("/tmp/shared", writer, &firstText) == lexxos::vfs::ReadStatus::Ok, "read first VFS tmp file");
	expect(second.readText("/tmp/shared", writer, &secondText) == lexxos::vfs::ReadStatus::Ok, "read second VFS tmp file");
	expect(firstText == "first", "first VFS keeps its tmp payload");
	expect(secondText == "second", "second VFS keeps its tmp payload");

	bool factoryCalled = false;
	auto const factory = [](lexxos::vfs::Vfs&, std::string_view, std::string_view, void* userData) {
		*static_cast<bool*>(userData) = true;
		return lexxos::vfs::MountStatus::Ok;
	};
	expect(first.registerFilesystemFactory("fake", factory, &factoryCalled), "register per-instance filesystem factory");
	expect(first.mount("fake", "/dev/fake0", "/mnt/fake") == lexxos::vfs::MountStatus::Ok, "invoke per-instance filesystem factory");
	expect(factoryCalled, "filesystem factory callback ran");
	expect(second.mount("fake", "/dev/fake0", "/mnt/fake") == lexxos::vfs::MountStatus::InvalidSource,
	    "second VFS does not inherit first VFS factory");
}

void checkGameBoyCartridgeUsesFakeClockAndFlashServices() {
	resetFakeState();
	lexxos::vfs::initialize(fakeServices());

	auto rom = makeTestRom();
	FakeBlockDevice romSource;
	StaticRomFilesystem romFilesystem{rom.data(), rom.size()};
	expect(lexxos::vfs::registerBlockDevice("rom0", &romSource), "register fake ROM block device");
	expect(lexxos::vfs::mountFilesystem("rom0", "/mnt/rom", &romFilesystem) == lexxos::vfs::MountStatus::Ok, "mount fake ROM filesystem");

	lexxos::gameboy::Cartridge cartridge{fakeServices()};
	auto const openResult = cartridge.open("/mnt/rom/fake.gb");
	expect(openResult.ok, "open fake Game Boy ROM");
	expect(g_sleepCalls == 0, "successful ROM open does not use retry sleep");

	std::uint8_t value = 0;
	expect(cartridge.read8(0x0150, value) == lexxos::vfs::ReadStatus::Ok, "read ROM byte through cartridge cache");
	expect(value == rom[0x0150], "cartridge read returns ROM byte");
	expect(cartridge.cacheFillCount() == 1, "cartridge filled cache from fake ROM filesystem");
	expect(cartridge.cacheFillMs() == 0, "fake uptime service is used for cache fill metrics");

	auto const flashResult = cartridge.loadRomToPlatformFlash();
	expect(flashResult.ok, "load ROM into fake platform flash");
	expect(g_flashProgramCalls == rom.size() / fakeFlashEraseSize, "fake flash erase/program service called per erase block");
	expect(cartridge.usingPlatformFlashRom(), "cartridge switches to fake platform flash ROM");
	expect(cartridge.platformFlashRomBytes() == rom.size(), "cartridge records fake flash ROM size");
	expect(std::memcmp(g_fakeFlash.data(), rom.data(), rom.size()) == 0, "fake flash contains ROM image");
}

} // namespace

namespace lexxos::apps {

// Only this contract-test executable needs the path helper used by mv::run.
bool resolvePath(lexxos::os::AppContext& context, std::string_view path,
    std::array<char, lexxos::vfs::maxPathLength>& output) {
	return lexxos::vfs::normalizePath(context.workingDirectory(), path, output.data(), output.size());
}

void writeUnsigned(lexxos::os::AppContext& context, std::uint32_t value) {
	writeUnsignedImpl(context, value);
}

void writeUnsigned64(lexxos::os::AppContext& context, std::uint64_t value) {
	writeUnsignedImpl(context, value);
}

} // namespace lexxos::apps

namespace lexxos::platform {

void initialize() {
}

BoardDescriptor const& board() {
	return fakeBoard();
}

void writeConsole(std::string_view text) {
	::writeConsole(text);
}

bool readConsoleChar(char& value) {
	return ::readConsoleChar(value);
}

void sleepMs(std::uint32_t milliseconds) {
	::sleepMs(milliseconds);
}

std::uint32_t uptimeMs() {
	return ::uptimeMs();
}

void setStatusLed(bool) {
}

bool configureGpioInput(GpioPin, GpioPull) {
	return false;
}

bool configureGpioOutput(GpioPin, bool) {
	return false;
}

bool readGpio(GpioPin, bool&) {
	return false;
}

bool writeGpio(GpioPin, bool) {
	return false;
}

bool configureGpioInterrupt(GpioPin, GpioInterruptEdge, GpioInterruptHandler, void*) {
	return false;
}

void disableGpioInterrupt(GpioPin) {
}

void reboot() {
}

bool gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	return findFlashRegion("gameboy-rom-cache", output);
}

bool eraseAndProgramGameBoyRomFlash(std::uint32_t offset, void const* data, std::size_t size) {
	return eraseAndProgramFlash(offset, data, size);
}

PlatformServices const& services() {
	return fakeServices();
}

} // namespace lexxos::platform

namespace lexxos::kernel {

void requestShutdown() {
}

void resetShutdownRequest() {
}

bool isShutdownRequested() {
	return false;
}

void sleepMs(std::uint32_t milliseconds) {
	::sleepMs(milliseconds);
}

bool createTask(TaskEntry entry, void* userData, TaskOptions) {
	if (entry == nullptr) {
		return false;
	}
	entry(userData);
	return true;
}

void startScheduler() {
}

void yield() {
}

std::size_t taskCount() {
	return 0;
}

void visitTasks(TaskVisitor, void*) {
}

MemorySnapshot memorySnapshot() {
	return MemorySnapshot{};
}

} // namespace lexxos::kernel

namespace lexxos::devices {

I2cBus& debugI2cBus() {
	static FakeI2cBus bus;
	return bus;
}

I2cBus& systemI2cBus() {
	return debugI2cBus();
}

char const* toString(SdCardState state) {
	switch (state) {
		case SdCardState::Ready:
			return "ready";
		case SdCardState::Uninitialized:
			return "uninitialized";
		default:
			return "other";
	}
}

char const* toString(SdCardType type) {
	switch (type) {
		case SdCardType::SdV2BlockAddressed:
			return "SDv2 block";
		case SdCardType::Unknown:
			return "unknown";
		default:
			return "other";
	}
}

} // namespace lexxos::devices

namespace lexxos::devices::sd_card {

bool hasCardDetect() {
	return false;
}

bool isCardInserted() {
	return false;
}

SdCardState state() {
	return SdCardState::Uninitialized;
}

SdCardType cardType() {
	return SdCardType::Unknown;
}

std::uint32_t blockSizeBytes() {
	return 0;
}

std::uint64_t blockCount() {
	return 0;
}

BlockDevice* blockDevice() {
	return nullptr;
}

BlockDevice* blockDevice(lexxos::platform::PlatformServices const&) {
	return nullptr;
}

} // namespace lexxos::devices::sd_card

int main() {
	checkRenameInheritsMoveRuntime();
	checkShellUsesInjectedConsoleService();
	checkExternalConsoleOutputPreservesShellInputLine();
	checkPathCompletionKeepsLongFileNames();
	checkShellAutosuggestUsesHistory();
	checkShellConfigPersistsHistoryAndAutosuggest();
	checkSensitiveShellHistory();
	checkStorageProgramUsesFakeServicesAndDeviceManager();
	checkPicoCalcSouthbridgeServiceLockUsesDeviceManager();
	checkRuntimeCapabilitiesFollowPlatformAndDeviceRegistration();
	checkVfsInstancesKeepTmpfsIsolated();
	checkGameBoyCartridgeUsesFakeClockAndFlashServices();
	return 0;
}
