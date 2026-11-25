#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/input_device.h>
#include <lexxos/external_program/external_program_runner.h>

namespace lexxos::external_program::detail {

constexpr std::uint32_t lexxSupportedCapabilities =
    LexxCapabilityConsoleText
    | LexxCapabilityKeyboard
    | LexxCapabilityStorageRead
    | LexxCapabilityStorageWrite
    | LexxCapabilityAudio
    | LexxCapabilityDevices
    | LexxCapabilityGraphics
    | LexxCapabilityClock;

constexpr std::uint32_t lexxGuestDeviceInfoSize = 40;
constexpr std::uint32_t lexxGuestDeviceRequestSize = 28;
constexpr std::uint32_t lexxGuestDeviceHandleBase = 0x100;
constexpr std::size_t lexxGuestDeviceSlots = 4;
constexpr std::uint32_t lexxGuestDeviceMaxTransfer = 256;

constexpr std::uint32_t deviceInvalid =
    static_cast<std::uint32_t>(-1);
constexpr std::uint32_t deviceNotFound =
    static_cast<std::uint32_t>(-2);
constexpr std::uint32_t deviceBusy =
    static_cast<std::uint32_t>(-3);
constexpr std::uint32_t deviceDenied =
    static_cast<std::uint32_t>(-4);
constexpr std::uint32_t deviceNotReady =
    static_cast<std::uint32_t>(-5);
constexpr std::uint32_t deviceUnsupported =
    static_cast<std::uint32_t>(-6);
constexpr std::uint32_t deviceIoError =
    static_cast<std::uint32_t>(-7);
constexpr std::uint32_t deviceNoHandles =
    static_cast<std::uint32_t>(-8);

constexpr std::uint32_t lexxGuestEventSize = 12;
constexpr std::uint32_t lexxHeaderReservedOffset = 44;
constexpr std::uint32_t lexxGuestDirEntrySize = 76;
constexpr std::uint32_t lexxGuestDirPageSize = 8;
constexpr std::uint32_t lexxGuestStatSize = 12;
constexpr std::uint32_t lexxGuestDirectoryNameSize = 64;
constexpr std::uint32_t lexxSyscallError = 0xFFFFFFFFU;
constexpr std::uint32_t lexxSyscallCancelled = 0xFFFFFFFEU;
constexpr std::uint32_t lexxGraphicsMaxText = 32U;
constexpr std::uint32_t lexxGraphicsMaxLinePixels = 1024U;

static_assert(lexxHeaderReservedOffset < lexxExecutableHeaderSize);

struct GuestGraphicsImage {
	std::uint16_t* pixels{nullptr}; // RGB565 and optional trailing alpha8 in one allocation
	std::uint8_t const* alpha{nullptr};
	std::uint16_t width{0};
	std::uint16_t height{0};
	std::uint32_t payloadBytes{0};
	std::uint32_t resourceOffset{0}; // Absolute payload offset in .lexx file.
	bool streamed{false};
	bool hasAlpha{false};
};

enum class VmSyscall : std::uint32_t {
	Exit = 0,
	Write = 1,
	ReadChar = 2,
	Read = 3,
	PollKeyEvent = 4,
	Yield = 5,
	Argc = 6,
	Argv = 7,
	OpenRead = 8,
	Close = 9,
	ListDirectory = 10,
	Stat = 11,
	Seek = 12,
	OpenWriteNew = 13,
	WriteFile = 14,
	RemoveFile = 15,
	RenamePath = 16,
	MakeDirectory = 17,
	RemoveDirectory = 18,
	Milliseconds = 19,
	PlayTone = 20,
	DeviceInfo = 21,
	DeviceOpen = 22,
	DeviceClose = 23,
	DeviceControl = 24,
	Graphics = 25,
	UnixTime = 26,
	MemoryInfo = 27,
	SleepMs = 28,
	InputMode = 29,
};

constexpr std::uint16_t readLe16(std::uint8_t const* data) {
	return static_cast<std::uint16_t>(data[0])
	    | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
}

constexpr std::uint32_t readLe32(std::uint8_t const* data) {
	return static_cast<std::uint32_t>(data[0])
	    | (static_cast<std::uint32_t>(data[1]) << 8U)
	    | (static_cast<std::uint32_t>(data[2]) << 16U)
	    | (static_cast<std::uint32_t>(data[3]) << 24U);
}

constexpr void writeLe16(std::uint8_t* data, std::uint16_t value) {
	data[0] = static_cast<std::uint8_t>(value & 0xFFU);
	data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

constexpr void writeLe32(std::uint8_t* data, std::uint32_t value) {
	data[0] = static_cast<std::uint8_t>(value & 0xFFU);
	data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	data[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
	data[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
    "Aligned RV32I guest access requires a little-endian host");

// The loader allocates guest memory with 4-byte alignment.
// Guest halfword/word operations validate address alignment
// before reaching these helpers.
//
// Explicit builtins remain available with -fno-builtin.
// memcpy preserves C++ object and strict-aliasing semantics.
template <typename T>
T readAlignedGuest(std::uint8_t const* data) {
	T value{};
	__builtin_memcpy(&value,
	    __builtin_assume_aligned(data, alignof(T)), sizeof(T));
	return value;
}

template <typename T>
void writeAlignedGuest(std::uint8_t* data, T value) {
	__builtin_memcpy(
	    __builtin_assume_aligned(data, alignof(T)),
	    &value, sizeof(T));
}
#endif

constexpr std::uint32_t alignDown4(std::uint32_t value) {
	return value & ~0x03U;
}

constexpr std::uint32_t alignUp4(std::uint32_t value) {
	return (value + 3U) & ~0x03U;
}

constexpr bool rangeFits(std::uint32_t offset, std::uint32_t length, std::uint32_t limit) {
	return offset <= limit && length <= limit - offset;
}

constexpr bool fileRangeFits(std::uint32_t offset, std::uint32_t length, std::uint64_t fileSize) {
	auto const wideOffset = static_cast<std::uint64_t>(offset);
	return wideOffset <= fileSize && static_cast<std::uint64_t>(length) <= fileSize - wideOffset;
}

constexpr bool bytesAreZero(std::uint8_t const* data, std::size_t size) {
	for (std::size_t index = 0; index < size; ++index) {
		if (data[index] != 0) {
			return false;
		}
	}

	return true;
}

constexpr std::size_t cstringLength(char const* text) {
	if (text == nullptr) {
		return 0;
	}

	std::size_t length = 0;
	while (text[length] != '\0') {
		++length;
	}

	return length;
}

constexpr std::int32_t signExtend(std::uint32_t value, std::uint8_t bits) {
	auto const signBit = 1U << (bits - 1U);
	return static_cast<std::int32_t>((value ^ signBit) - signBit);
}

constexpr std::int32_t immediateI(std::uint32_t instruction) {
	return signExtend(instruction >> 20U, 12);
}

constexpr std::int32_t immediateS(std::uint32_t instruction) {
	auto const value = ((instruction >> 7U) & 0x1FU) | (((instruction >> 25U) & 0x7FU) << 5U);
	return signExtend(value, 12);
}

constexpr std::int32_t immediateB(std::uint32_t instruction) {
	auto const value = (((instruction >> 31U) & 0x01U) << 12U)
	    | (((instruction >> 7U) & 0x01U) << 11U)
	    | (((instruction >> 25U) & 0x3FU) << 5U)
	    | (((instruction >> 8U) & 0x0FU) << 1U);
	return signExtend(value, 13);
}

constexpr std::int32_t immediateJ(std::uint32_t instruction) {
	auto const value = (((instruction >> 31U) & 0x01U) << 20U)
	    | (((instruction >> 12U) & 0xFFU) << 12U)
	    | (((instruction >> 20U) & 0x01U) << 11U)
	    | (((instruction >> 21U) & 0x3FFU) << 1U);
	return signExtend(value, 21);
}

constexpr std::uint32_t immediateU(std::uint32_t instruction) {
	return instruction & 0xFFFFF000U;
}

bool isCtrlCEvent(lexxos::devices::InputEvent const& event);

void writeHexNibble(lexxos::os::AppContext& context, std::uint8_t value);

void writeHex32(lexxos::os::AppContext& context, std::uint32_t value);

bool parseHeader(std::uint8_t const* data, std::size_t size, LexxExecutableHeader& output);

ExternalProgramStatus validateHeader(
    LexxExecutableHeader const& header,
    std::uint64_t fileSize,
    bool reservedBytesAreZero);

} // namespace lexxos::external_program::detail
