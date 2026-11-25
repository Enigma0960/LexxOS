#include <lexxos/external_program/detail/rv32i_support.h>

#include <lexxos/os/app_context.h>

namespace lexxos::external_program::detail {

bool isCtrlCEvent(lexxos::devices::InputEvent const& event) {
	if (event.type != lexxos::devices::InputEventType::KeyDown) {
		return false;
	}

	if (event.character == 0x03U) {
		return true;
	}

	return event.key == lexxos::devices::KeyCode::C
	    && (event.modifiers & lexxos::devices::InputModifierCtrl) != 0;
}

void writeHexNibble(lexxos::os::AppContext& context, std::uint8_t value) {
	char const text[] = {static_cast<char>(value < 10 ? ('0' + value) : ('a' + value - 10)), '\0'};
	context.write(text);
}

void writeHex32(lexxos::os::AppContext& context, std::uint32_t value) {
	context.write("0x");
	for (int shift = 28; shift >= 0; shift -= 4) {
		writeHexNibble(context, static_cast<std::uint8_t>((value >> static_cast<unsigned>(shift)) & 0x0FU));
	}
}

bool parseHeader(std::uint8_t const* data, std::size_t size, LexxExecutableHeader& output) {
	if (size < lexxExecutableHeaderSize) {
		return false;
	}

	output = {
	    .formatVersion = readLe16(data + 4),
	    .headerSize = readLe16(data + 6),
	    .machine = readLe16(data + 8),
	    .abiVersion = readLe16(data + 10),
	    .flags = readLe32(data + 12),
	    .requiredCapabilities = readLe32(data + 16),
	    .entryOffset = readLe32(data + 20),
	    .imageOffset = readLe32(data + 24),
	    .imageSize = readLe32(data + 28),
	    .memorySize = readLe32(data + 32),
	    .stackSize = readLe32(data + 36),
	    .bssSize = readLe32(data + 40),
	};
	return readLe32(data) == lexxExecutableMagic;
}

ExternalProgramStatus validateHeader(
    LexxExecutableHeader const& header,
    std::uint64_t fileSize,
    bool reservedBytesAreZero) {
	if (header.formatVersion != lexxExecutableFormatVersion) {
		return ExternalProgramStatus::UnsupportedVersion;
	}

	if (header.machine != lexxMachineRv32i) {
		return ExternalProgramStatus::UnsupportedMachine;
	}

	if (header.abiVersion != lexxExecutableAbiVersion) {
		return ExternalProgramStatus::UnsupportedAbi;
	}

	if ((header.requiredCapabilities & ~lexxSupportedCapabilities) != 0U) {
		return ExternalProgramStatus::UnsupportedCapabilities;
	}

	if ((header.flags & ~lexxExecutableFlagInputEvents) != 0U || !reservedBytesAreZero) {
		return ExternalProgramStatus::InvalidHeader;
	}

	if (header.headerSize != lexxExecutableHeaderSize || header.imageOffset < header.headerSize) {
		return ExternalProgramStatus::InvalidHeader;
	}

	if ((header.memorySize & 0x03U) != 0U) {
		return ExternalProgramStatus::InvalidHeader;
	}

	if (header.imageSize == 0 || header.memorySize == 0 || header.imageSize > header.memorySize) {
		return ExternalProgramStatus::InvalidImage;
	}

	if (header.memorySize > lexxMaximumGuestMemoryBytes) {
		return ExternalProgramStatus::ImageTooLarge;
	}

	if (!fileRangeFits(header.imageOffset, header.imageSize, fileSize)) {
		return ExternalProgramStatus::InvalidImage;
	}

	if ((header.entryOffset & 0x03U) != 0U || !rangeFits(header.entryOffset, 4U, header.memorySize)) {
		return ExternalProgramStatus::InvalidImage;
	}

	if (header.stackSize > header.memorySize - header.imageSize) {
		return ExternalProgramStatus::InvalidImage;
	}

	return ExternalProgramStatus::Ok;
}

} // namespace lexxos::external_program::detail
