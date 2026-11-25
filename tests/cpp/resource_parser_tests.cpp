#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <lexxos/external_program/lexx_resources.h>
#include <lexxos/platform/platform_services.h>

namespace {

namespace resources = lexxos::external_program::resources;
using Bytes = std::vector<std::uint8_t>;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "resource parser: " << message << '\n';
		std::exit(1);
	}
}

void append32(Bytes& bytes, std::uint32_t value) {
	for (unsigned shift = 0; shift < 32; shift += 8) {
		bytes.push_back(static_cast<std::uint8_t>(value >> shift));
	}
}

void append(Bytes& bytes, std::string_view text) {
	bytes.insert(bytes.end(), text.begin(), text.end());
}

std::uint32_t crc(Bytes const& bytes) {
	std::uint32_t result = 0xFFFFFFFFU;
	for (auto byte : bytes) {
		result ^= byte;
		for (unsigned bit = 0; bit < 8; ++bit) {
			result = (result >> 1U) ^ ((result & 1U) ? 0xEDB88320U : 0U);
		}
	}

	return result ^ 0xFFFFFFFFU;
}

Bytes package(bool indexed, bool corruptCrc = false, bool corruptOffset = false) {
	Bytes result(16, 0);
	append(result, "payload");
	Bytes table;
	append32(table, 16);
	append32(table, 7);
	std::string_view const alias = ":/assets/a.txt";
	table.push_back(static_cast<std::uint8_t>(alias.size()));
	table.push_back(0);
	append(table, alias);
	if (indexed) {
		append32(table, corruptOffset ? 1U : 0U);
	}

	result.insert(result.end(), table.begin(), table.end());
	append(result, indexed ? "LXR2" : "LXRS");
	append32(result, 1);
	append32(result, static_cast<std::uint32_t>(table.size()));
	append32(result, crc(table) ^ (corruptCrc ? 1U : 0U));
	append(result, "{}");
	append(result, "LXMF");
	append32(result, 2);
	append32(result, 0);
	return result;
}

lexxos::vfs::FileHandle openPackage(lexxos::vfs::Vfs& vfs, Bytes const& bytes) {
	expect(vfs.writeText("/tmp/package.lexx", {reinterpret_cast<char const*>(bytes.data()), bytes.size()})
	        == lexxos::vfs::WriteStatus::Ok,
	    "store package fixture");
	lexxos::vfs::FileHandle file;
	expect(vfs.open("/tmp/package.lexx", lexxos::vfs::OpenMode::Read, file)
	        == lexxos::vfs::OpenStatus::Ok,
	    "open package fixture");
	return file;
}

} // namespace

void checkResourceParser() {
	lexxos::platform::PlatformServices platform{};
	lexxos::vfs::Vfs vfs;
	vfs.initialize(platform);
	for (bool indexed : {false, true}) {
		auto file = openPackage(vfs, package(indexed));
		resources::Index index;
		expect(resources::inspect(file, 16, index) == resources::InspectStatus::Valid, "valid index rejected");
		expect(index.indexed == indexed && index.count == 1, "index format or count changed");
		resources::Entry entry;
		expect(resources::find(file, index, ":/assets/a.txt", entry), "resource missing");
		expect(entry.offset == 16 && entry.size == 7, "resource bounds changed");
		expect(!resources::find(file, index, ":/missing", entry), "missing resource found");
		expect(resources::lookup(file, index, ":/assets", entry) == resources::PathKind::Directory, "implicit directory missing");
		expect(resources::lookup(file, index, ":/assets/a.txt", entry) == resources::PathKind::File, "file lookup changed");
		expect(resources::lookup(file, index, ":/assets/a.txt/", entry) == resources::PathKind::Missing, "file accepted as directory");
		std::uint32_t written = 0;
		expect(resources::listDirectory(file, index, ":/assets", 0, 1, written,
		           [&](std::string_view name, bool directory, std::uint32_t size) {
			           expect(name == "a.txt" && !directory && size == 7, "directory entry changed");
		           })
		        && written == 1,
		    "directory listing changed");
		file.close();
		auto corrupt = openPackage(vfs, package(indexed, true));
		expect(resources::inspect(corrupt, 16, index) == resources::InspectStatus::Invalid, "CRC corruption accepted");
	}
	auto invalidOffset = openPackage(vfs, package(true, false, true));
	resources::Index index;
	expect(resources::inspect(invalidOffset, 16, index) == resources::InspectStatus::Invalid, "nonzero first index offset accepted");
	expect(!resources::validAlias(":/../secret") && !resources::validAlias(":/assets//a"), "invalid aliases accepted");
	expect(resources::validAlias(":/assets/a.txt"), "valid alias rejected");
}
