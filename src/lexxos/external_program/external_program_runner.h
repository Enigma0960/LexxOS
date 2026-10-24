#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::external_program {

inline constexpr std::uint32_t lexxExecutableMagic = 0x5858454CU; // "LEXX", little-endian
inline constexpr std::uint16_t lexxExecutableFormatVersion = 1;
inline constexpr std::uint16_t lexxExecutableAbiVersion = 1;
inline constexpr std::uint16_t lexxMachineRv32i = 1;
inline constexpr std::uint16_t lexxExecutableHeaderSize = 64;
inline constexpr std::uint32_t lexxMaximumGuestMemoryBytes = 64U * 1024U;
inline constexpr std::uint32_t lexxMaximumGuestArguments = 32U;
inline constexpr std::uint32_t lexxGuestFileDescriptorBase = 3U;
inline constexpr std::size_t lexxMaximumGuestOpenFiles = 4U;
inline constexpr std::uint32_t lexxUnlimitedInstructionLimit = 0U;
inline constexpr std::uint32_t lexxDefaultInstructionLimit = lexxUnlimitedInstructionLimit;

struct LexxExecutableHeader {
	std::uint16_t formatVersion{0};
	std::uint16_t headerSize{0};
	std::uint16_t machine{0};
	std::uint16_t abiVersion{0};
	std::uint32_t flags{0};
	std::uint32_t requiredCapabilities{0};
	std::uint32_t entryOffset{0};
	std::uint32_t imageOffset{0};
	std::uint32_t imageSize{0};
	std::uint32_t memorySize{0};
	std::uint32_t stackSize{0};
	std::uint32_t bssSize{0};
};

enum LexxCapability : std::uint32_t {
	LexxCapabilityConsoleText = 1U << 0U,
	LexxCapabilityKeyboard = 1U << 1U,
	LexxCapabilityStorageRead = 1U << 2U,
	LexxCapabilityStorageWrite = 1U << 3U,
	LexxCapabilityNetwork = 1U << 4U,
	LexxCapabilityDevices = 1U << 5U,
	LexxCapabilityAudio = 1U << 6U,
};

enum class ExternalProgramStatus : std::uint8_t {
	Ok,
	Cancelled,
	OpenFailed,
	ReadFailed,
	ShortHeader,
	BadMagic,
	UnsupportedVersion,
	UnsupportedMachine,
	UnsupportedAbi,
	UnsupportedCapabilities,
	InvalidHeader,
	InvalidImage,
	ImageTooLarge,
	ArgumentsTooLarge,
	MemoryUnavailable,
	InstructionLimit,
	InstructionAddressFault,
	LoadAddressFault,
	StoreAddressFault,
	IllegalInstruction,
	UnsupportedSyscall,
};

struct ExternalProgramOptions {
	// 0 means unlimited. Interactive programs are expected to stop through exit, Ctrl+C or stop <id>.
	std::uint32_t maxInstructions{lexxDefaultInstructionLimit};
	bool trace{false};
	int guestArgc{0};
	char const* const* guestArgv{nullptr};
};

struct ExternalProgramResult {
	ExternalProgramStatus status{ExternalProgramStatus::Ok};
	int exitCode{0};
	std::uint32_t instructionsExecuted{0};
	std::uint32_t faultAddress{0};
	std::uint32_t faultInstruction{0};
	std::uint32_t unsupportedSyscall{0};

	[[nodiscard]] bool ok() const {
		return status == ExternalProgramStatus::Ok && exitCode == 0;
	}
};

std::string_view toString(ExternalProgramStatus status);

ExternalProgramResult runLexxExecutable(
    lexxos::os::AppContext& context,
    std::string_view path,
    ExternalProgramOptions const& options = {});

} // namespace lexxos::external_program
