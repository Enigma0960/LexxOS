#include <lexxos/vfs/detail/vfs_pseudo_files.h>

#include <array>
#include <cstdint>
#include <span>

#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/detail/vfs_string.h>
#include <lexxos/vfs/vfs_internal_types.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

namespace {

constexpr std::array<std::string_view, 5> rootDirectories{"dev", "mnt", "proc", "sys", "tmp"};
constexpr std::array<std::string_view, 3> procFiles{"meminfo", "tasks", "uptime"};
constexpr std::array<std::string_view, 2> sysFiles{"board", "storage"};

void write(lexxos::vfs::TextWriter writer, void* userData, std::string_view text) {
	writer(text, userData);
}

void writeLine(lexxos::vfs::TextWriter writer, void* userData, std::string_view text) {
	writer(text, userData);
	writer("\r\n", userData);
}

void writeUnsigned(lexxos::vfs::TextWriter writer, void* userData, std::uint32_t value) {
	std::array<char, 16> buffer{};
	std::size_t size = 0;

	if (value == 0) {
		write(writer, userData, "0");
		return;
	}

	while (value != 0 && size < buffer.size()) {
		buffer[size++] = static_cast<char>('0' + value % 10);
		value /= 10;
	}

	while (size > 0) {
		char const character = buffer[--size];
		write(writer, userData, std::string_view{&character, 1});
	}
}

void writeUnsigned64(lexxos::vfs::TextWriter writer, void* userData, std::uint64_t value) {
	std::array<char, 32> buffer{};
	std::size_t size = 0;

	if (value == 0) {
		write(writer, userData, "0");
		return;
	}

	while (value != 0 && size < buffer.size()) {
		buffer[size++] = static_cast<char>('0' + value % 10);
		value /= 10;
	}

	while (size > 0) {
		char const character = buffer[--size];
		write(writer, userData, std::string_view{&character, 1});
	}
}

bool listEntries(
    std::span<std::string_view const> entries,
    lexxos::vfs::DirectoryVisitor visitor,
    void* userData) {
	for (auto const entry : entries) {
		if (!lexxos::vfs::detail::visitEntry(
		        visitor,
		        userData,
		        entry,
		        lexxos::vfs::NodeType::Directory)) {
			return false;
		}
	}
	return true;
}

bool listFiles(
    std::span<std::string_view const> entries,
    lexxos::vfs::DirectoryVisitor visitor,
    void* userData) {
	for (auto const entry : entries) {
		if (!lexxos::vfs::detail::visitEntry(
		        visitor,
		        userData,
		        entry,
		        lexxos::vfs::NodeType::RegularFile)) {
			return false;
		}
	}
	return true;
}

void readProcMemInfo(lexxos::vfs::TextWriter writer, void* userData) {
	auto const memory = lexxos::kernel::memorySnapshot();
	if (!memory.available) {
		writeLine(writer, userData, "MemAvailable: n/a");
		return;
	}
	write(writer, userData, "MemFree: ");
	writeUnsigned(writer, userData, memory.freeHeapBytes);
	writeLine(writer, userData, " bytes");
	write(writer, userData, "MemFreeMin: ");
	writeUnsigned(writer, userData, memory.minimumEverFreeHeapBytes);
	writeLine(writer, userData, " bytes");
}

void readProcTasks(lexxos::vfs::TextWriter writer, void* userData) {
	lexxos::vfs::detail::TextWriterContext writerContext{.writer = writer, .userData = userData};
	writeLine(writer, userData, "name priority stack free started class core");
	lexxos::kernel::visitTasks(
	    [](lexxos::kernel::TaskSnapshot const& task, void* callbackData) {
		    auto const& target = *static_cast<lexxos::vfs::detail::TextWriterContext const*>(callbackData);
		    write(target.writer, target.userData, task.name);
		    write(target.writer, target.userData, " ");
		    writeUnsigned(target.writer, target.userData, task.priority);
		    write(target.writer, target.userData, " ");
		    writeUnsigned(target.writer, target.userData, task.stackSizeBytes);
		    write(target.writer, target.userData, " ");
		    writeUnsigned(target.writer, target.userData, task.stackFreeBytes);
		    write(target.writer, target.userData, " ");
		    write(target.writer, target.userData, task.started ? "yes" : "no");
		    write(target.writer, target.userData, " ");
		    write(target.writer, target.userData, lexxos::kernel::toString(task.executionClass));
		    write(target.writer, target.userData, " ");
		    if (task.coreAffinity < 0) {
			    writeLine(target.writer, target.userData, "any");
		    } else {
			    writeUnsigned(target.writer, target.userData, static_cast<std::uint32_t>(task.coreAffinity));
			    writeLine(target.writer, target.userData, "");
		    }
	    },
	    &writerContext);
}

void readProcUptime(
    lexxos::platform::PlatformServices const& platform,
    lexxos::vfs::TextWriter writer,
    void* userData) {
	auto const& clock = platform.clock;
	writeUnsigned(writer, userData, clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U);
	writeLine(writer, userData, " ms");
}

void readSysBoard(
    lexxos::platform::PlatformServices const& platform,
    lexxos::vfs::TextWriter writer,
    void* userData) {
	auto const& boardService = platform.board;
	auto const& board = boardService.current != nullptr ? boardService.current() : lexxos::platform::board();
	write(writer, userData, "id: ");
	writeLine(writer, userData, board.boardId);
	write(writer, userData, "name: ");
	writeLine(writer, userData, board.boardName);
	write(writer, userData, "vendor: ");
	writeLine(writer, userData, board.vendor);
	write(writer, userData, "processor: ");
	write(writer, userData, board.processorName);
	write(writer, userData, " / ");
	writeLine(writer, userData, lexxos::platform::toString(board.processorFamily));
}

void readSysStorage(lexxos::vfs::TextWriter writer, void* userData) {
#if FEATURE_ENABLED(SD_CARD)
	write(writer, userData, "card detect: ");
	if (lexxos::devices::sd_card::hasCardDetect()) {
		writeLine(writer, userData, lexxos::devices::sd_card::isCardInserted() ? "inserted" : "empty");
	} else {
		writeLine(writer, userData, "not-configured");
	}
	write(writer, userData, "sd state: ");
	writeLine(writer, userData, lexxos::devices::toString(lexxos::devices::sd_card::state()));
	write(writer, userData, "sd type: ");
	writeLine(writer, userData, lexxos::devices::toString(lexxos::devices::sd_card::cardType()));
	write(writer, userData, "block size: ");
	writeUnsigned(writer, userData, lexxos::devices::sd_card::blockSizeBytes());
	writeLine(writer, userData, " bytes");
	write(writer, userData, "block count: ");
	writeUnsigned64(writer, userData, lexxos::devices::sd_card::blockCount());
	writeLine(writer, userData, "");
#else
	writeLine(writer, userData, "sd support: disabled");
#endif
}

} // namespace

void lexxos::vfs::Vfs::initializePseudoFiles(lexxos::platform::PlatformServices const& platform) {
	_platformServices = &platform;
}

bool lexxos::vfs::Vfs::isKnownDirectory(std::string_view path) const {
	return detail::equals(path, "/") || detail::equals(path, "/dev") || detail::equals(path, "/mnt") || detail::equals(path, "/proc") || detail::equals(path, "/sys") || detail::equals(path, "/tmp");
}

bool lexxos::vfs::Vfs::isKnownStaticFile(std::string_view path) const {
	return detail::equals(path, "/proc/meminfo") || detail::equals(path, "/proc/tasks") || detail::equals(path, "/proc/uptime") || detail::equals(path, "/sys/board") || detail::equals(path, "/sys/storage") || detail::equals(path, "/dev/null");
}

bool lexxos::vfs::Vfs::statPseudoNode(std::string_view path, Stat& output) {
	if (isKnownDirectory(path)) {
		output.type = NodeType::Directory;
		output.ready = true;
		return true;
	}
	if (isKnownStaticFile(path)) {
		output.type = NodeType::RegularFile;
		output.ready = true;
		return true;
	}
	return false;
}

bool lexxos::vfs::Vfs::listPseudoDirectory(
    std::string_view path,
    DirectoryVisitor visitor,
    void* userData) {
	if (detail::equals(path, "/")) {
		return listEntries(rootDirectories, visitor, userData);
	}
	if (detail::equals(path, "/proc")) {
		return listFiles(procFiles, visitor, userData);
	}
	if (detail::equals(path, "/sys")) {
		return listFiles(sysFiles, visitor, userData);
	}
	return false;
}

lexxos::vfs::ReadStatus lexxos::vfs::Vfs::readPseudoText(
    std::string_view path,
    TextWriter writer,
    void* userData) {
	if (isKnownDirectory(path)) {
		return ReadStatus::IsDirectory;
	}
	if (detail::equals(path, "/dev/null")) {
		return ReadStatus::Ok;
	}
	if (detail::equals(path, "/proc/meminfo")) {
		readProcMemInfo(writer, userData);
		return ReadStatus::Ok;
	}
	if (detail::equals(path, "/proc/tasks")) {
		readProcTasks(writer, userData);
		return ReadStatus::Ok;
	}
	if (detail::equals(path, "/proc/uptime")) {
		readProcUptime(platformServices(), writer, userData);
		return ReadStatus::Ok;
	}
	if (detail::equals(path, "/sys/board")) {
		readSysBoard(platformServices(), writer, userData);
		return ReadStatus::Ok;
	}
	if (detail::equals(path, "/sys/storage")) {
		readSysStorage(writer, userData);
		return ReadStatus::Ok;
	}
	return ReadStatus::NotFound;
}
