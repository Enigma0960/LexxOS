#include <lexxos/apps/fs/hexdump_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
	using HexdumpBuffer = std::array<char, 16>;

	constexpr std::uint32_t hexdumpWorkspaceBytes =
	    static_cast<std::uint32_t>(lexxos::vfs::maxPathLength + sizeof(HexdumpBuffer) + alignof(HexdumpBuffer));

	constexpr lexxos::program::ProgramArgumentSpec hexdumpArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional readable VFS file; stdin is used when omitted",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("hexdump: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("hexdump: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& path, HexdumpBuffer*& buffer) {
		return allocateWorkspaceObject(context, path) && allocateWorkspaceObject(context, buffer);
	}

	bool dumpInputStream(
	    lexxos::os::AppContext& context,
	    char const* commandName,
	    lexxos::io::InputStream& input,
	    HexdumpBuffer& buffer) {
		std::uint32_t offset = 0;
		while (true) {
			// Read the next output row.
			std::size_t bytesRead = 0;
			auto const readStatus = input.read(buffer.data(), buffer.size(), bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				context.write(commandName);
				context.writeLine(": read failed");
				return false;
			}
			if (bytesRead == 0) {
				break;
			}

			// Print the offset and fixed-width hexadecimal columns.
			writeHex32(context, offset);
			context.write(": ");
			for (std::size_t index = 0; index < buffer.size(); ++index) {
				if (index < bytesRead) {
					writeHexByte(context, static_cast<std::uint8_t>(buffer[index]));
				} else {
					context.write("  ");
				}
				context.write(index == 7 ? "  " : " ");
			}

			// Append printable ASCII characters for the bytes in this row.
			context.write("|");
			for (std::size_t index = 0; index < bytesRead; ++index) {
				auto const value = static_cast<std::uint8_t>(buffer[index]);
				char const character = isPrintableAscii(value) ? static_cast<char>(value) : '.';
				context.write(std::string_view{&character, 1});
			}
			context.writeLine("|");

			offset += static_cast<std::uint32_t>(bytesRead);
		}
		return true;
	}

} // namespace

ProgramDescriptor HexdumpProgram::descriptor() {
	return {
	    .name = "hexdump",
	    .help = "print file or stdin as hexadecimal bytes",
	    .description = "Displays file or standard input data as 16-byte hexadecimal rows with an ASCII representation.",
	    .usage = "hexdump [path]",
	    .arguments = argumentList(hexdumpArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = hexdumpWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int HexdumpProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	HexdumpBuffer* buffer = nullptr;
	if (!allocateWorkspace(context, path, buffer)) {
		return 1;
	}

	// Use standard input when no file path was supplied.
	std::string_view const requestedPath = arguments.positional("path");
	if (requestedPath.empty()) {
		return dumpInputStream(context, "hexdump", context.standardInput(), *buffer) ? 0 : 1;
	}

	// Resolve and open the requested file as an input stream.
	if (!resolvePath(context, requestedPath, *path)) {
		return 1;
	}

	lexxos::io::FileInputStream input{path->data()};
	if (!input.ok()) {
		context.write("hexdump: ");
		context.write(path->data());
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(input.status()));
		return 1;
	}
	return dumpInputStream(context, "hexdump", input, *buffer) ? 0 : 1;
}

} // namespace lexxos::apps::filesystem_programs
