#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <lexxos/apps/fs/detail/file_info.h>
#include <lexxos/apps/fs/detail/lexx_info.h>
#include <lexxos/apps/fs/detail/manifest_printer.h>
#include <lexxos/apps/fs/info_program.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/program/program_memory_arena.h>

namespace {

using lexxos::apps::filesystem_programs::InfoProgram;
namespace info = lexxos::apps::filesystem_programs::detail;
using Bytes = std::vector<std::uint8_t>;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "info: " << message << '\n';
		std::exit(1);
	}
}

class InfoOutput final : public lexxos::io::OutputStream {
public:
	bool write(std::string_view value) override {
		text.append(value);
		return true;
	}

	std::string text;
};

void put32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
	for (unsigned i = 0; i < 4; ++i) {
		bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
	}
}

std::uint32_t crc(std::string_view text) {
	std::uint32_t value = 0xFFFFFFFF;
	for (unsigned char byte : text) {
		value ^= byte;
		for (unsigned i = 0; i < 8; ++i) {
			value = (value >> 1U) ^ ((value & 1U) ? 0xEDB88320U : 0U);
		}
	}

	return value ^ 0xFFFFFFFF;
}

Bytes package(std::string_view manifest, bool footer = true) {
	Bytes bytes(68, 0);
	put32(bytes, 0, lexxos::external_program::lexxExecutableMagic);
	bytes[4] = 1;
	bytes[6] = 64;
	bytes[8] = 1;
	bytes[10] = 1;
	put32(bytes, 24, 64);
	put32(bytes, 28, 4);
	put32(bytes, 32, 4096);
	put32(bytes, 36, 2048);
	if (footer) {
		bytes.insert(bytes.end(), manifest.begin(), manifest.end());
		auto offset = bytes.size();
		bytes.resize(offset + 12);
		bytes[offset] = 'L';
		bytes[offset + 1] = 'X';
		bytes[offset + 2] = 'M';
		bytes[offset + 3] = 'F';
		put32(bytes, offset + 4, static_cast<std::uint32_t>(manifest.size()));
		put32(bytes, offset + 8, crc(manifest));
	}

	return bytes;
}

void store(lexxos::vfs::Vfs& vfs, char const* path, std::string_view data) {
	expect(vfs.writeText(path, data) == lexxos::vfs::WriteStatus::Ok, "store fixture");
}

void store(lexxos::vfs::Vfs& vfs, char const* path, Bytes const& data) {
	store(vfs, path, {reinterpret_cast<char const*>(data.data()), data.size()});
}

} // namespace

void checkInfoProgram() {
	lexxos::platform::PlatformServices services{};
	lexxos::vfs::Vfs leftVfs, rightVfs;
	leftVfs.initialize(services);
	rightVfs.initialize(services);
	lexxos::os::OperatingSystem left{services, leftVfs}, right{services, rightVfs};
	InfoOutput output;
	lexxos::program::ProgramMemoryArena arena;
	auto context = left.createAppContext({.standardOutput = &output, .standardError = &output, .memoryArena = &arena});
	auto other = right.createAppContext({.standardOutput = &output, .standardError = &output, .memoryArena = &arena});
	auto run = [&](lexxos::os::AppContext& active, char const* path) {
		arena.release();
		expect(arena.reserve(InfoProgram::descriptor().runtime) == lexxos::program::ProgramMemoryArenaStatus::Ready, "reserve descriptor workspace");
		output.text.clear();
		char const* argv[]{"info", path};
		lexxos::program::ProgramInvocation invocation{2, argv};
		lexxos::program::ParsedArguments args;
		expect(args.addPositional("path", path), "set path argument");
		return InfoProgram::run(active, invocation, args);
	};
	store(leftVfs, "/tmp/hello.txt", "hello\n");
	expect(run(context, "/tmp/hello.txt") == 0, "text inspection");
	expect(output.text == "name: hello.txt\npath: /tmp/hello.txt\ntype: text\nsize: 6 bytes\ncreated: \nmodified: \npermissions: \n", "metadata output compatibility");
	expect(run(context, "/tmp/missing") == 1 && output.text.find("cannot access") != std::string::npos, "missing file");
	expect(run(context, "/tmp") == 0 && output.text.find("files: 1\ndirectories: 0") != std::string::npos, "directory count");

	store(leftVfs, "/tmp/test.lnk", "LEXX-LINK-1\ntarget=/left.lexx\nmode=foreground\ndelay_ms=0\n");
	store(rightVfs, "/tmp/test.lnk", "LEXX-LINK-1\ntarget=/right.lexx\nmode=foreground\ndelay_ms=0\n");
	expect(run(context, "/tmp/test.lnk") == 0 && output.text.find("target: /left.lexx") != std::string::npos, "left shortcut VFS");
	expect(run(other, "/tmp/test.lnk") == 0 && output.text.find("target: /right.lexx") != std::string::npos, "right shortcut VFS");
	store(leftVfs, "/tmp/test.lnk", "broken");
	expect(run(context, "/tmp/test.lnk") == 1 && output.text.find("shortcut (invalid)") != std::string::npos, "invalid shortcut");

	store(leftVfs, "/tmp/app.LEXX", package(R"({"name":"demo","nested":{"value":-1.25e+3},"items":[1,true]})"));
	expect(run(context, "/tmp/app.LEXX") == 0, "uppercase extension and valid manifest");
	expect(output.text.find("manifest: present (CRC32 OK)\nmanifest.name: demo\nmanifest.nested.value: -1.25e+3\nmanifest.items: [array]") != std::string::npos, "manifest output");
	auto bytes = package("{}");
	bytes.back() ^= 1;
	store(leftVfs, "/tmp/app.LEXX", bytes);
	expect(run(context, "/tmp/app.LEXX") == 1 && output.text.find("CRC32 mismatch") != std::string::npos, "corrupt CRC");
	store(leftVfs, "/tmp/app.LEXX", package("", false));
	expect(run(context, "/tmp/app.LEXX") == 0 && output.text.find("not present (legacy)") != std::string::npos, "legacy executable");
	bytes = package("{}");
	put32(bytes, 24, 0xFFFFFFFF);
	store(leftVfs, "/tmp/app.LEXX", bytes);
	expect(run(context, "/tmp/app.LEXX") == 1 && output.text.find("lexx: invalid header") != std::string::npos, "overflowed image extent");
	bytes = package("{}");
	put32(bytes, bytes.size() - 8, 4097);
	store(leftVfs, "/tmp/app.LEXX", bytes);
	expect(run(context, "/tmp/app.LEXX") == 1 && output.text.find("invalid location or length") != std::string::npos, "oversized manifest");
	for (auto json : {R"({"ok":1,"bad":})", R"({"bad":01})", R"({"bad":1+2})", R"({"bad":1e})"}) {
		store(leftVfs, "/tmp/app.LEXX", package(json));
		expect(run(context, "/tmp/app.LEXX") == 1 && output.text.find("invalid JSON") != std::string::npos, "invalid JSON rejected");
		expect(output.text.find("manifest.ok:") == std::string::npos, "no partial manifest emission");
	}
	store(leftVfs, "/tmp/app.LEXX", package("{}"));
	arena.release();
	output.text.clear();
	expect(info::inspectLexxInfo(context, "/tmp/app.LEXX") == 1 && output.text.find("not enough workspace") != std::string::npos, "bounded workspace failure");

	lexxos::vfs::Stat metadata{.type = lexxos::vfs::NodeType::RegularFile, .sizeBytes = 6};
	metadata.hasFatAttributes = true;
	metadata.fatAttributes = 0x23;
	metadata.modifiedAt = {.year = 2026, .month = 11, .day = 18, .hour = 7, .minute = 8, .second = 9, .valid = true};
	output.text.clear();
	expect(info::inspectFileInfo(context, "/tmp/hello.txt", metadata) == 0, "FAT metadata");
	expect(output.text.find("modified: 2026-11-18 07:08:09") != std::string::npos && output.text.find("fat attributes: read-only, hidden, archive") != std::string::npos, "timestamp and attributes format");
}
