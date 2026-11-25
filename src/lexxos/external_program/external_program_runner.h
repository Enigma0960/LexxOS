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
// Manifest input.mode="events" is compiled into this header flag so the VM
// does not need to allocate a JSON parser for every .lexx launch.
inline constexpr std::uint32_t lexxExecutableFlagInputEvents = 1U << 0U;
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
	LexxCapabilityGraphics = 1U << 7U,
	LexxCapabilityClock = 1U << 8U,
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
	InvalidResources,
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

enum class Rv32iBenchmarkCase : std::uint8_t {
	Alu,
	Branch,
	Memory,
	Ecall,
	ZeroRegister,
};

enum class Rv32iHostBenchmarkCase : std::uint8_t {
	RegisterAlu,
	DdrReadWrite,
};

enum class Rv32iOpcodeForm : std::uint8_t {
	Register,
	Immediate,
	Load,
	Store,
	Branch,
	Jump,
	JumpRegister,
	LoadUpper,
	AddUpperPc,
	Fence,
	MixedAluMemory,
	MixedBranchMemory,
	MixedSyscall,
	LoopControl,
};

struct Rv32iOpcodeProbe {
	Rv32iOpcodeForm form{};
	std::uint8_t funct3{0};
	std::uint8_t funct7{0};
	std::int32_t lhs{0};
	std::int32_t rhs{0};
	std::int32_t immediate{0};
	std::int32_t contribution{1};
};

struct Rv32iOpcodeResult;

struct ExternalProgramResult {
	ExternalProgramStatus status{ExternalProgramStatus::Ok};
	int exitCode{0};
	std::uint32_t instructionsExecuted{0};
	std::uint32_t faultAddress{0};
	std::uint32_t faultInstruction{0};
	std::uint32_t unsupportedSyscall{0};
	// Diagnostics for a failed guest reservation; zero if unknown.
	std::uint32_t requiredMemoryBytes{0};
	std::uint32_t availableMemoryBytes{0};

	[[nodiscard]] bool ok() const {
		return status == ExternalProgramStatus::Ok && exitCode == 0;
	}
};

struct Rv32iOpcodeResult {
	ExternalProgramResult program{};
	std::uint32_t instructionsPerIteration{0};
	std::uint32_t fixedInstructions{0};
	int expectedExit{0};
};

Rv32iOpcodeResult benchmarkRv32iOpcode(
    lexxos::os::AppContext& context,
    Rv32iOpcodeProbe const& probe,
    std::uint32_t iterations,
    std::uint8_t* scratch,
    std::size_t scratchSize);

std::string_view toString(ExternalProgramStatus status);

ExternalProgramResult runLexxExecutable(
    lexxos::os::AppContext& context,
    std::string_view path,
    ExternalProgramOptions const& options = {});

// Executes a synthetic program in the production RV32I VM.
// The caller owns the scratch buffer. No VFS access is performed.
ExternalProgramResult benchmarkRv32i(
    lexxos::os::AppContext& context,
    Rv32iBenchmarkCase test,
    std::uint32_t iterations,
    std::uint8_t* scratch,
    std::size_t scratchSize);

// Runs a host-side reference workload in the same optimized
// translation unit as the production interpreter.
std::uint32_t benchmarkRv32iHost(
    Rv32iHostBenchmarkCase test, std::uint32_t iterations);

} // namespace lexxos::external_program
