#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/shell/shell.h>

namespace lexxos::io {

lexxos::vfs::ReadStatus NullInputStream::read(void*, std::size_t, std::size_t& bytesRead) {
	bytesRead = 0;
	return lexxos::vfs::ReadStatus::Ok;
}

bool OutputStream::writeLine(std::string_view text) {
	return write(text) && write("\n");
}

ConsoleOutputStream::ConsoleOutputStream()
    : ConsoleOutputStream(lexxos::platform::services()) {
}

ConsoleOutputStream::ConsoleOutputStream(lexxos::platform::PlatformServices const& platform)
    : _platform(&platform) {
}

bool ConsoleOutputStream::write(std::string_view text) {
	if (_platform != nullptr && _platform->console.write != nullptr) {
		_platform->console.write(text);
	}
	return true;
}

bool NullOutputStream::write(std::string_view) {
	return true;
}

InputStream& nullInputStream() {
	static NullInputStream stream;
	return stream;
}

OutputStream& consoleOutputStream() {
	static ConsoleOutputStream stream;
	return stream;
}

OutputStream& nullOutputStream() {
	static NullOutputStream stream;
	return stream;
}

} // namespace lexxos::io

namespace lexxos::devices {

char const* toString(SdCardState state) {
	switch (state) {
		case SdCardState::Uninitialized:
			return "uninitialized";
		case SdCardState::Ready:
			return "ready";
		case SdCardState::NoBus:
			return "no-bus";
		case SdCardState::NoCard:
			return "no-card";
		case SdCardState::Unsupported:
			return "unsupported";
		case SdCardState::Timeout:
			return "timeout";
		case SdCardState::IoError:
			return "io-error";
		case SdCardState::CardRemoved:
			return "card-removed";
	}
	return "unknown";
}

char const* toString(SdCardType type) {
	switch (type) {
		case SdCardType::Unknown:
			return "unknown";
		case SdCardType::SdV1:
			return "sd-v1";
		case SdCardType::SdV2ByteAddressed:
			return "sd-v2-byte";
		case SdCardType::SdV2BlockAddressed:
			return "sd-v2-block";
	}
	return "unknown";
}

namespace sd_card {

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

} // namespace sd_card

} // namespace lexxos::devices

namespace lexxos::platform {

bool readConsoleChar(char&) {
	return false;
}

BoardDescriptor const& board() {
	static constexpr BoardDescriptor descriptor{};
	return descriptor;
}

char const* toString(ProcessorFamily family) {
	switch (family) {
		case ProcessorFamily::NativeHost:
			return "native-host";
		case ProcessorFamily::Rp2040:
			return "rp2040";
		case ProcessorFamily::Rp2350:
			return "rp2350";
		case ProcessorFamily::Esp32:
			return "esp32";
		case ProcessorFamily::Stm32f4:
			return "stm32f4";
		case ProcessorFamily::Unknown:
			return "unknown";
	}
	return "unknown";
}

PlatformServices const& services() {
	static constexpr PlatformServices platformServices{
	    .console = {
	                .readChar = readConsoleChar,
	                },
	};
	return platformServices;
}

} // namespace lexxos::platform

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions) {
	if (entry == nullptr) {
		return false;
	}
	entry(userData);
	return true;
}

void startScheduler() {
}

void sleepMs(std::uint32_t) {
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

namespace lexxos::shell {

bool Shell::setWorkingDirectory(std::string_view) {
	return true;
}

} // namespace lexxos::shell
