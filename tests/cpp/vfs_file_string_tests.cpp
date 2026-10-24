#include <lexxos/text/string_utils.h>
#include <lexxos/vfs/file_number.h>
#include <lexxos/vfs/file_string.h>
#include <lexxos/vfs/vfs.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "vfs file helper test failed: " << message << '\n';
		std::exit(1);
	}
}

void checkFixedStringReadAddsNullTerminator() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	expect(vfs.writeText("/tmp/header", "LEX0payload") == lexxos::vfs::WriteStatus::Ok, "write tmp header");

	lexxos::vfs::FileHandle file;
	expect(vfs.open("/tmp/header", lexxos::vfs::OpenMode::Read, file) == lexxos::vfs::OpenStatus::Ok,
	    "open tmp header");

	char header[5]{};
	auto const result = lexxos::vfs::readString(file, header, 4);
	expect(result.status == lexxos::vfs::ReadStatus::Ok, "fixed string read succeeds");
	expect(result.bytesRead == 4, "fixed string read returns requested size");
	expect(result.full(), "fixed string read reports full result");
	expect(header[4] == '\0', "fixed string read appends null terminator");
	expect(lexxos::text::equals(result.view(header), "LEX0"), "fixed string read view contains header");
}

void checkFixedStringReadCanSkipNullTerminator() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	expect(vfs.writeText("/tmp/header", "ABCDpayload") == lexxos::vfs::WriteStatus::Ok, "write tmp header");

	char header[4]{};
	auto const result = lexxos::vfs::readString(
	    vfs,
	    "/tmp/header",
	    0,
	    header,
	    4,
	    lexxos::vfs::FileStringTerminator::None);
	expect(result.status == lexxos::vfs::ReadStatus::Ok, "raw fixed string read succeeds");
	expect(result.bytesRead == 4, "raw fixed string read returns requested size");
	expect(std::memcmp(header, "ABCD", 4) == 0, "raw fixed string read preserves four bytes");
}

void checkShortReadIsVisibleToCaller() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	expect(vfs.writeText("/tmp/short", "AB") == lexxos::vfs::WriteStatus::Ok, "write short tmp file");

	char value[5]{};
	auto const result = lexxos::vfs::readString(vfs, "/tmp/short", 0, value, 4);
	expect(result.status == lexxos::vfs::ReadStatus::Ok, "short fixed string read succeeds");
	expect(result.bytesRead == 2, "short fixed string read returns available size");
	expect(result.shortRead(), "short fixed string read reports short result");
	expect(value[2] == '\0', "short fixed string read terminates at actual byte count");
	expect(lexxos::text::equals(result.view(value), "AB"), "short fixed string view contains available data");
}

void checkBufferSizeValidation() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	expect(vfs.writeText("/tmp/header", "ABCD") == lexxos::vfs::WriteStatus::Ok, "write tmp header");

	char value[4]{};
	auto const result = lexxos::vfs::readString(
	    vfs,
	    "/tmp/header",
	    0,
	    value,
	    sizeof(value),
	    lexxos::vfs::FileStringReadOptions{
	        .byteCount = 4,
	        .terminator = lexxos::vfs::FileStringTerminator::Null,
	    });
	expect(result.status == lexxos::vfs::ReadStatus::InvalidPath, "null-terminated read rejects small buffer");
	expect(result.bufferTooSmall, "small buffer is reported explicitly");
}

void checkNumberReadSupportsFixedWidthIntegers() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	static constexpr std::uint8_t data[]{
	    0x7F,
	    0xFE,
	    0x34,
	    0x12,
	    0xF0,
	    0xFF,
	    0x78,
	    0x56,
	    0x34,
	    0x12,
	    0x12,
	    0x34,
	    0x56,
	    0x78,
	    0x08,
	    0x07,
	    0x06,
	    0x05,
	    0x04,
	    0x03,
	    0x02,
	    0x01,
	};
	expect(vfs.writeText(
	           "/tmp/numbers",
	           std::string_view{reinterpret_cast<char const*>(data), sizeof(data)})
	        == lexxos::vfs::WriteStatus::Ok,
	    "write binary number file");

	lexxos::vfs::FileHandle file;
	expect(vfs.open("/tmp/numbers", lexxos::vfs::OpenMode::Read, file) == lexxos::vfs::OpenStatus::Ok,
	    "open binary number file");

	std::uint8_t u8 = 0;
	auto result = lexxos::vfs::readUInt8(file, u8);
	expect(result.full() && u8 == 0x7F, "read uint8 value");

	std::int8_t i8 = 0;
	result = lexxos::vfs::readInt8(file, i8);
	expect(result.full() && i8 == static_cast<std::int8_t>(-2), "read int8 value");

	std::uint16_t u16 = 0;
	result = lexxos::vfs::readUInt16(file, u16, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && u16 == 0x1234U, "read little-endian uint16 value");

	std::int16_t i16 = 0;
	result = lexxos::vfs::readInt16(file, i16, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && i16 == static_cast<std::int16_t>(-16), "read little-endian int16 value");

	std::uint32_t u32 = 0;
	result = lexxos::vfs::readUInt32(file, u32, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && u32 == 0x12345678UL, "read little-endian uint32 value");

	u32 = 0;
	result = lexxos::vfs::readUInt32(file, u32, lexxos::vfs::FileNumberByteOrder::BigEndian);
	expect(result.full() && u32 == 0x12345678UL, "read big-endian uint32 value");

	std::uint64_t u64 = 0;
	result = lexxos::vfs::readUInt64(file, u64, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && u64 == 0x0102030405060708ULL, "read little-endian uint64 value");
}

void checkNumberReadSupportsFloatingPoint() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	static constexpr std::uint8_t data[]{
	    0x00,
	    0x00,
	    0x80,
	    0x3F,
	    0x40,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x40,
	};
	expect(vfs.writeText(
	           "/tmp/floats",
	           std::string_view{reinterpret_cast<char const*>(data), sizeof(data)})
	        == lexxos::vfs::WriteStatus::Ok,
	    "write binary float file");

	lexxos::vfs::FileHandle file;
	expect(vfs.open("/tmp/floats", lexxos::vfs::OpenMode::Read, file) == lexxos::vfs::OpenStatus::Ok,
	    "open binary float file");

	float f32 = 0.0F;
	auto result = lexxos::vfs::readFloat32(file, f32, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && f32 == 1.0F, "read little-endian float32 value");

	f32 = 0.0F;
	result = lexxos::vfs::readFloat32(file, f32, lexxos::vfs::FileNumberByteOrder::BigEndian);
	expect(result.full() && f32 == 2.0F, "read big-endian float32 value");

	double f64 = 0.0;
	result = lexxos::vfs::readFloat64(file, f64, lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && f64 == 2.0, "read little-endian float64 value");
}

void checkNumberReadFromPathAndOffset() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	static constexpr std::uint8_t data[]{0xAA, 0xBB, 0x44, 0x33, 0x22, 0x11};
	expect(vfs.writeText(
	           "/tmp/offset-number",
	           std::string_view{reinterpret_cast<char const*>(data), sizeof(data)})
	        == lexxos::vfs::WriteStatus::Ok,
	    "write offset number file");

	std::uint32_t value = 0;
	auto const result = lexxos::vfs::readNumber(
	    vfs,
	    "/tmp/offset-number",
	    2,
	    value,
	    lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.full() && value == 0x11223344UL, "read number from path and offset");
}

void checkShortNumberReadLeavesOutputUntouched() {
	lexxos::vfs::Vfs vfs;
	vfs.initialize();
	static constexpr std::uint8_t data[]{0x01, 0x02};
	expect(vfs.writeText(
	           "/tmp/short-number",
	           std::string_view{reinterpret_cast<char const*>(data), sizeof(data)})
	        == lexxos::vfs::WriteStatus::Ok,
	    "write short number file");

	std::uint32_t value = 0xDEADBEEFUL;
	auto const result = lexxos::vfs::readNumber(
	    vfs,
	    "/tmp/short-number",
	    0,
	    value,
	    lexxos::vfs::FileNumberByteOrder::LittleEndian);
	expect(result.status == lexxos::vfs::ReadStatus::Ok, "short number read returns underlying read status");
	expect(result.shortRead(), "short number read is visible to caller");
	expect(!result.valueReady(), "short number read does not expose a ready value");
	expect(value == 0xDEADBEEFUL, "short number read leaves output unchanged");
}

void checkStringHelpers() {
	expect(lexxos::text::equals("LexxOS", "LexxOS"), "equals detects identical strings");
	expect(!lexxos::text::equals("LexxOS", "lexxos"), "equals remains case-sensitive");
	expect(lexxos::text::equalsIgnoreCaseAscii("LexxOS", "lexxos"), "case-insensitive ASCII compare works");
	expect(lexxos::text::compare("abc", "abd") < 0, "compare reports lower value");
	expect(lexxos::text::compare("abd", "abc") > 0, "compare reports greater value");
	expect(lexxos::text::compare("abc", "abc") == 0, "compare reports equal value");
	expect(lexxos::text::startsWith("/mnt/sd/rom.gb", "/mnt"), "startsWith detects prefix");
	expect(lexxos::text::endsWith("/mnt/sd/rom.gb", ".gb"), "endsWith detects suffix");
	expect(lexxos::text::contains("/mnt/sd/rom.gb", "sd"), "contains detects substring");
	expect(lexxos::text::trim(" \tLexxOS\r\n") == std::string_view{"LexxOS"}, "trim removes ASCII whitespace");

	char output[7]{};
	expect(lexxos::text::copy(output, sizeof(output), "LexxOS"), "copy writes null-terminated string");
	expect(lexxos::text::equals(output, "LexxOS"), "copy result is comparable");
	expect(!lexxos::text::copy(output, 4, "LexxOS"), "copy rejects small buffer");
}

} // namespace

int main() {
	checkNumberReadSupportsFixedWidthIntegers();
	checkNumberReadSupportsFloatingPoint();
	checkNumberReadFromPathAndOffset();
	checkShortNumberReadLeavesOutputUntouched();
	checkFixedStringReadAddsNullTerminator();
	checkFixedStringReadCanSkipNullTerminator();
	checkShortReadIsVisibleToCaller();
	checkBufferSizeValidation();
	checkStringHelpers();
	return 0;
}
