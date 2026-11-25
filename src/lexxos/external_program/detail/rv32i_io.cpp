#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

void Rv32iVirtualMachine::handleWriteSyscall() {
	auto const fd = _registers[10];
	auto const address = _registers[11];
	auto const length = _registers[12];
	if (!hasCapability(LexxCapabilityConsoleText)
	    || (fd != 1U && fd != 2U)
	    || !rangeValid(address, length)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	auto* output = fd == 2U ? &_context.standardError() : &_context.standardOutput();
	std::uint32_t written = 0;
	while (written < length) {
		auto const chunk = std::min<std::uint32_t>(length - written, 128U);
		output->write(std::string_view{reinterpret_cast<char const*>(_memory + address + written), chunk});
		written += chunk;
	}

	_registers[10] = written;
}

void Rv32iVirtualMachine::handleReadCharSyscall() {
	// In Events mode, read_char() must never re-read the same physical key
	// from console_input or race Shell for physical UART bytes.
	if (_inputMode == lexxos::app::program_input::InputMode::Events) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (!hasCapability(LexxCapabilityKeyboard) || !_context.hasProgramFocus()) {
		_registers[10] = lexxSyscallError;
		return;
	}

	char value = 0;
	if (!_context.consoleInput().readChar(_context.platform(), value)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (static_cast<std::uint8_t>(value) == 0x03U) {
		_context.requestCancellation();
		_status = ExternalProgramStatus::Cancelled;
		_running = false;
		_registers[10] = lexxSyscallCancelled;
		return;
	}

	_registers[10] = static_cast<std::uint8_t>(value);
}

void Rv32iVirtualMachine::handleReadSyscall() {
	auto const fd = _registers[10];
	auto const address = _registers[11];
	auto const length = _registers[12];
	if (length == 0) {
		_registers[10] = 0;
		return;
	}

	if (!rangeValid(address, length)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (fd == 0U) {
		if (!hasCapability(LexxCapabilityKeyboard)) {
			_registers[10] = lexxSyscallError;
			return;
		}

		std::size_t bytesRead = 0;
		auto const status = _context.standardInput().read(_memory + address, length, bytesRead);
		_registers[10] = status == lexxos::vfs::ReadStatus::Ok ? static_cast<std::uint32_t>(bytesRead) : lexxSyscallError;
		return;
	}

	std::size_t index = 0;
	if (!guestFileIndex(fd, index) || !_guestFiles[index].isOpen() || !_guestFiles[index].canRead()) {
		_registers[10] = lexxSyscallError;
		return;
	}

	std::size_t requested = length;
	if (_resourceStreams[index].active) {
		auto& stream = _resourceStreams[index];
		requested = std::min<std::size_t>(requested, stream.size - stream.position);
		if (requested == 0) {
			_registers[10] = 0;
			return;
		}

		if (!_guestFiles[index].seek(static_cast<std::uint64_t>(stream.offset) + stream.position)) {
			_registers[10] = lexxSyscallError;
			return;
		}
	}

	std::size_t bytesRead = 0;
	auto const status = _guestFiles[index].read(_memory + address, requested, bytesRead);
	if (status == lexxos::vfs::ReadStatus::Ok && _resourceStreams[index].active) {
		_resourceStreams[index].position += static_cast<std::uint32_t>(bytesRead);
	}

	_registers[10] = status == lexxos::vfs::ReadStatus::Ok ? static_cast<std::uint32_t>(bytesRead) : lexxSyscallError;
}

void Rv32iVirtualMachine::handlePollKeyEventSyscall() {
	auto const address = _registers[10];
	if (!hasCapability(LexxCapabilityKeyboard) || !rangeValid(address, lexxGuestEventSize)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (!_context.hasProgramFocus()) {
		_registers[10] = 0;
		return;
	}

	lexxos::devices::InputEvent event{};
	while (_context.programInput().pollEvent(_guestInputCursor, event)) {
		if (isCtrlCEvent(event)) {
			_context.requestCancellation();
			_status = ExternalProgramStatus::Cancelled;
			_running = false;
			_registers[10] = lexxSyscallCancelled;
			return;
		}

		_memory[address] = event.type == lexxos::devices::InputEventType::KeyDown ? 1U : 2U;
		_memory[address + 1U] = event.modifiers;
		_memory[address + 2U] = event.character;
		_memory[address + 3U] = event.scanCode;
		writeLe16(_memory + address + 4U, static_cast<std::uint16_t>(event.key));
		writeLe16(_memory + address + 6U, 0);
		writeLe32(_memory + address + 8U, event.timestampMs);
		_registers[10] = 1;
		return;
	}

	_registers[10] = 0;
}

void Rv32iVirtualMachine::handleListDirectorySyscall() {
	auto const guestAddress = _registers[10];
	auto const offset = _registers[11];
	auto const bufferAddress = _registers[12];
	if (!rangeValid(bufferAddress, lexxGuestDirEntrySize * lexxGuestDirPageSize)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> guestPath{};
	std::array<char, lexxos::vfs::maxPathLength> path{};

	if (!loadGuestString(guestAddress, guestPath)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (std::string_view{guestPath.data()}.starts_with(":/")) {
		lexxos::vfs::FileHandle package{};
		if (_context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, package)
		    != lexxos::vfs::OpenStatus::Ok) {
			_registers[10] = lexxSyscallError;
			return;
		}

		std::memset(_memory + bufferAddress, 0, lexxGuestDirEntrySize * lexxGuestDirPageSize);
		std::uint32_t count = 0;
		auto const ok = resources::listDirectory(package, _resourceIndex, guestPath.data(),
		    offset, lexxGuestDirPageSize, count,
		    [&](std::string_view name, bool isDirectory, std::uint32_t size) {
			    auto* record = _memory + bufferAddress + count * lexxGuestDirEntrySize;
			    std::memcpy(record, name.data(), name.size());
			    writeLe32(record + 64, size);
			    record[72] = static_cast<std::uint8_t>(isDirectory
			            ? lexxos::vfs::NodeType::Directory
			            : lexxos::vfs::NodeType::RegularFile);
		    });

		_registers[10] = ok ? count : lexxSyscallError;
		return;
	}

	if (!hasCapability(LexxCapabilityStorageRead)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (!_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), path.data(), path.size())) {
		_registers[10] = lexxSyscallError;
		return;
	}

	lexxos::vfs::Stat stat{};
	if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::Directory) {
		_registers[10] = lexxSyscallError;
		return;
	}

	struct Page {
		std::uint8_t* bytes;
		std::uint32_t skip;
		std::uint32_t seen{0};
		std::uint32_t count{0};
	} page{_memory + bufferAddress, offset};

	std::memset(page.bytes, 0, lexxGuestDirEntrySize * lexxGuestDirPageSize);

	auto const visitor = [](lexxos::vfs::DirectoryEntry const& entry, void* opaque) -> bool {
		auto& out = *static_cast<Page*>(opaque);
		if (out.seen < out.skip) {
			++out.seen;
			return true;
		}

		if (out.count >= lexxGuestDirPageSize) {
			return true;
		}

		auto* record = out.bytes + out.count * lexxGuestDirEntrySize;
		auto const length = std::min<std::size_t>(entry.name.size(), lexxGuestDirectoryNameSize - 1U);
		std::memcpy(record, entry.name.data(), length);
		record[length] = 0;
		writeLe32(record + 64, static_cast<std::uint32_t>(entry.sizeBytes));
		writeLe32(record + 68, static_cast<std::uint32_t>(entry.sizeBytes >> 32U));
		record[72] = static_cast<std::uint8_t>(entry.type);
		++out.count;
		return true;
	};
	_registers[10] = _context.vfs().list(path.data(), visitor, &page)
	    ? page.count
	    : lexxSyscallError;
}

void Rv32iVirtualMachine::handleStatSyscall() {
	auto const guestAddress = _registers[10];
	auto const bufferAddress = _registers[11];
	if (!rangeValid(bufferAddress, lexxGuestStatSize)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> guestPath{};
	std::array<char, lexxos::vfs::maxPathLength> path{};

	if (!loadGuestString(guestAddress, guestPath)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (std::string_view{guestPath.data()}.starts_with(":/")) {
		lexxos::vfs::FileHandle package{};
		resources::Entry resource{};
		if (_context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, package) != lexxos::vfs::OpenStatus::Ok) {
			_registers[10] = lexxSyscallError;
			return;
		}

		auto const kind = resources::lookup(package, _resourceIndex, guestPath.data(), resource);
		if (kind == resources::PathKind::Missing) {
			_registers[10] = lexxSyscallError;
			return;
		}

		auto* record = _memory + bufferAddress;
		writeLe32(record, static_cast<std::uint32_t>(kind == resources::PathKind::Directory ? lexxos::vfs::NodeType::Directory : lexxos::vfs::NodeType::RegularFile));
		writeLe32(record + 4, kind == resources::PathKind::Directory ? 0U : resource.size);
		writeLe32(record + 8, 0);
		_registers[10] = 0;
		return;
	}

	if (!hasCapability(LexxCapabilityStorageRead)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	if (!_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), path.data(), path.size())) {
		_registers[10] = lexxSyscallError;
		return;
	}

	lexxos::vfs::Stat stat{};
	if (!_context.vfs().stat(path.data(), stat)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	auto* record = _memory + bufferAddress;
	writeLe32(record, static_cast<std::uint32_t>(stat.type));
	writeLe32(record + 4, static_cast<std::uint32_t>(stat.sizeBytes));
	writeLe32(record + 8, static_cast<std::uint32_t>(stat.sizeBytes >> 32U));
	_registers[10] = 0;
}

void Rv32iVirtualMachine::handleOpenReadSyscall() {
	std::array<char, lexxos::vfs::maxPathLength> guestPath{};
	if (!loadGuestString(_registers[10], guestPath)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	auto const isResource = std::string_view{guestPath.data()}.starts_with(":/");
	if (!isResource && !hasCapability(LexxCapabilityStorageRead)) {
		_registers[10] = lexxSyscallError;
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> normalizedPath{};
	if (!isResource && !_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), normalizedPath.data(), normalizedPath.size())) {
		_registers[10] = lexxSyscallError;
		return;
	}

	std::size_t slot = _guestFiles.size();
	for (std::size_t index = 0; index < _guestFiles.size(); ++index) {
		if (!_guestFiles[index].isOpen()) {
			slot = index;
			break;
		}
	}

	if (slot >= _guestFiles.size()) {
		_registers[10] = lexxSyscallError;
		return;
	}

	_resourceStreams[slot] = {};
	if (isResource) {
		resources::Entry entry{};
		if (!_resourceIndex.present
		    || _context.vfs().open(_executablePath, lexxos::vfs::OpenMode::Read, _guestFiles[slot]) != lexxos::vfs::OpenStatus::Ok
		    || !_guestFiles[slot].isOpen()
		    || !_guestFiles[slot].canRead()
		    || !resources::find(_guestFiles[slot], _resourceIndex, guestPath.data(), entry)) {
			_guestFiles[slot].close();
			_registers[10] = lexxSyscallError;
			return;
		}

		_resourceStreams[slot] = {.active = true, .offset = entry.offset, .size = entry.size};
	} else {
		auto const openStatus = _context.vfs().open(normalizedPath.data(), lexxos::vfs::OpenMode::Read, _guestFiles[slot]);
		if (openStatus != lexxos::vfs::OpenStatus::Ok || !_guestFiles[slot].isOpen() || !_guestFiles[slot].canRead()) {
			_guestFiles[slot].close();
			_registers[10] = lexxSyscallError;
			return;
		}
	}

	_registers[10] = lexxGuestFileDescriptorBase + static_cast<std::uint32_t>(slot);
}

void Rv32iVirtualMachine::handleCloseSyscall() {
	std::size_t index = 0;
	if (!guestFileIndex(_registers[10], index) || !_guestFiles[index].isOpen()) {
		_registers[10] = lexxSyscallError;
		return;
	}

	_guestFiles[index].close();
	_resourceStreams[index] = {};
	_registers[10] = 0;
}

void Rv32iVirtualMachine::handleSeekSyscall() {
	std::size_t index = 0;
	auto const fd = _registers[10];

	if (!guestFileIndex(fd, index) || !_guestFiles[index].isOpen() || !_guestFiles[index].canRead() || (!_resourceStreams[index].active && !hasCapability(LexxCapabilityStorageRead))) {
		_registers[10] = lexxSyscallError;
		return;
	}

	auto const position = static_cast<std::uint64_t>(_registers[11])
	    | (static_cast<std::uint64_t>(_registers[12]) << 32U);

	if (_resourceStreams[index].active) {
		if (position > _resourceStreams[index].size) {
			_registers[10] = lexxSyscallError;
			return;
		}

		_resourceStreams[index].position = static_cast<std::uint32_t>(position);
		_registers[10] = 0;
		return;
	}

	_registers[10] = (position <= _guestFiles[index].size() && _guestFiles[index].seek(position))
	    ? 0U
	    : lexxSyscallError;
}

bool Rv32iVirtualMachine::guestPath(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& out) {
	std::array<char, lexxos::vfs::maxPathLength> raw{};
	return loadGuestString(address, raw)
	    && _context.vfs().normalizePath(_context.workingDirectory(), raw.data(), out.data(), out.size());
}

void Rv32iVirtualMachine::handleOpenWriteNewSyscall() {
	auto const guestAddress = _registers[10];
	_registers[10] = lexxSyscallError;
	if (!hasCapability(LexxCapabilityStorageWrite)) {
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!guestPath(guestAddress, path)) {
		return;
	}

	lexxos::vfs::Stat existing{};
	if (_context.vfs().stat(path.data(), existing)) {
		return;
	}

	std::size_t slot = _guestFiles.size();
	for (std::size_t i = 0; i < _guestFiles.size(); ++i) {
		if (!_guestFiles[i].isOpen()) {
			slot = i;
			break;
		}
	}

	if (slot == _guestFiles.size()) {
		return;
	}

	if (_context.vfs().open(path.data(), lexxos::vfs::OpenMode::CreateTruncateWrite,
	        _guestFiles[slot])
	        != lexxos::vfs::OpenStatus::Ok
	    || !_guestFiles[slot].canWrite()) {
		_guestFiles[slot].close();
		return;
	}

	_registers[10] = lexxGuestFileDescriptorBase + static_cast<std::uint32_t>(slot);
}

void Rv32iVirtualMachine::handleWriteFileSyscall() {
	auto const fd = _registers[10];
	auto const address = _registers[11];
	auto const length = _registers[12];
	_registers[10] = lexxSyscallError;
	std::size_t index = 0;
	if (!hasCapability(LexxCapabilityStorageWrite) || length > 4096U || !rangeValid(address, length) || !guestFileIndex(fd, index) || !_guestFiles[index].canWrite()) {
		return;
	}

	auto const data = std::string_view{reinterpret_cast<char const*>(_memory + address), length};
	if (_guestFiles[index].write(data) == lexxos::vfs::WriteStatus::Ok) {
		_registers[10] = length;
	}
}

void Rv32iVirtualMachine::handleRemoveFileSyscall() {
	auto const guestAddress = _registers[10];
	_registers[10] = lexxSyscallError;
	if (!hasCapability(LexxCapabilityStorageWrite)) {
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!guestPath(guestAddress, path)) {
		return;
	}

	lexxos::vfs::Stat stat{};
	if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::RegularFile) {
		return;
	}

	if (_context.vfs().remove(path.data()) == lexxos::vfs::RemoveStatus::Ok) {
		_registers[10] = 0;
	}
}

void Rv32iVirtualMachine::handleRenamePathSyscall() {
	auto const first = _registers[10];
	auto const second = _registers[11];
	_registers[10] = lexxSyscallError;
	if (!hasCapability(LexxCapabilityStorageWrite)) {
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> source{};
	std::array<char, lexxos::vfs::maxPathLength> destination{};
	if (!guestPath(first, source) || !guestPath(second, destination)) {
		return;
	}

	lexxos::vfs::Stat existing{};
	if (_context.vfs().stat(destination.data(), existing)) {
		return;
	}

	auto const status = _context.vfs().rename(source.data(), destination.data());
	if (status == lexxos::vfs::RenameStatus::Ok) {
		_registers[10] = 0;
	} else if (status == lexxos::vfs::RenameStatus::CrossFilesystem
	    || status == lexxos::vfs::RenameStatus::CrossDirectory) {
		_registers[10] = 0xFFFFFFFEU;
	}
}

void Rv32iVirtualMachine::handleMakeDirectorySyscall() {
	auto const guestAddress = _registers[10];
	auto const recursive = _registers[11] != 0U;
	// mkdir error codes are part of the guest SDK ABI (negative signed ints).
	constexpr std::int32_t failed = -1;
	constexpr std::int32_t permissionDenied = -2;
	constexpr std::int32_t invalidPath = -3;
	constexpr std::int32_t notFound = -4;
	constexpr std::int32_t alreadyExists = -5;
	constexpr std::int32_t notDirectory = -6;
	constexpr std::int32_t readOnly = -7;
	constexpr std::int32_t noSpace = -8;
	constexpr std::int32_t tooLarge = -9;
	constexpr std::int32_t blockDevice = -10;
	auto const setResult = [&](std::int32_t status) {
		_registers[10] = static_cast<std::uint32_t>(status);
	};
	auto const writeError = [](lexxos::vfs::WriteStatus status) -> std::int32_t {
		switch (status) {
			case lexxos::vfs::WriteStatus::Ok                : return 0;
			case lexxos::vfs::WriteStatus::NotFound          : return notFound;
			case lexxos::vfs::WriteStatus::IsDirectory       : return alreadyExists;
			case lexxos::vfs::WriteStatus::IsBlockDevice     : return blockDevice;
			case lexxos::vfs::WriteStatus::ReadOnlyFilesystem: return readOnly;
			case lexxos::vfs::WriteStatus::NotWritable       : return permissionDenied;
			case lexxos::vfs::WriteStatus::InvalidPath       : return invalidPath;
			case lexxos::vfs::WriteStatus::NoSpace           : return noSpace;
			case lexxos::vfs::WriteStatus::TooLarge          : return tooLarge;
		}

		return failed;
	};
	setResult(invalidPath);
	if (!hasCapability(LexxCapabilityStorageWrite)) {
		setResult(permissionDenied);
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!guestPath(guestAddress, path)) {
		return;
	}

	// Check every component in the host VFS; no recursive traversal in guest SDK.
	auto const createOne = [&](char const* component, bool allowExisting) -> std::int32_t {
		lexxos::vfs::Stat existing{};
		if (_context.vfs().stat(component, existing)) {
			if (existing.type == lexxos::vfs::NodeType::Directory) {
				return allowExisting ? 0 : alreadyExists;
			}

			return existing.type == lexxos::vfs::NodeType::BlockDevice
			    ? blockDevice
			    : notDirectory;
		}

		return writeError(_context.vfs().createDirectory(component));
	};

	if (!recursive) {
		setResult(createOne(path.data(), false));
		return;
	}

	// normalizePath() already produced an absolute, bounded, NUL-terminated path.
	// Keep intermediate directories intact on failure, as with mkdir -p.
	for (std::size_t index = 1; index < path.size(); ++index) {
		if (path[index] != '/' && path[index] != '\0') {
			continue;
		}

		if (index > 1 && path[index - 1] == '/') {
			if (path[index] == '\0') {
				break;
			}

			continue;
		}

		auto const terminator = path[index];
		path[index] = '\0';
		auto const status = createOne(path.data(), true);
		path[index] = terminator;
		if (status != 0) {
			setResult(status);
			return;
		}

		if (terminator == '\0') {
			setResult(0);
			return;
		}
	}

	setResult(tooLarge);
}

void Rv32iVirtualMachine::handleRemoveDirectorySyscall() {
	auto const guestAddress = _registers[10];
	_registers[10] = lexxSyscallError;
	if (!hasCapability(LexxCapabilityStorageWrite)) {
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!guestPath(guestAddress, path)) {
		return;
	}

	lexxos::vfs::Stat stat{};
	if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::Directory) {
		return;
	}

	if (_context.vfs().removeDirectory(path.data()) == lexxos::vfs::RemoveStatus::Ok) {
		_registers[10] = 0;
	}
}

} // namespace lexxos::external_program::detail
