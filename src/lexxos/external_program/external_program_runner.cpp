#include <lexxos/external_program/external_program_runner.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/guest_graphics_mode.h>
#include <lexxos/app/program_input.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/bus/spi_transfer.h>
#include <lexxos/devices/bus/uart_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/external_program/detail/rv32i_virtual_machine.h>
#include <lexxos/external_program/lexx_graphics_asset.h>
#include <lexxos/external_program/lexx_resources.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/ui/guest_ui.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::external_program {

namespace {

	using detail::bytesAreZero;
	using detail::lexxHeaderReservedOffset;
	using detail::parseHeader;
	using detail::Rv32iVirtualMachine;
	using detail::validateHeader;
	using detail::writeLe32;

	ExternalProgramStatus readExact(lexxos::vfs::FileHandle& file, void* buffer, std::size_t length) {
		auto* output = static_cast<std::uint8_t*>(buffer);
		std::size_t total = 0;
		while (total < length) {
			std::size_t bytesRead = 0;
			auto const status = file.read(output + total, length - total, bytesRead);
			if (status != lexxos::vfs::ReadStatus::Ok) {
				return ExternalProgramStatus::ReadFailed;
			}

			if (bytesRead == 0) {
				return ExternalProgramStatus::ReadFailed;
			}

			total += bytesRead;
		}

		return ExternalProgramStatus::Ok;
	}

	ExternalProgramResult runVirtualMachine(
	    lexxos::os::AppContext& context,
	    std::uint8_t* memory,
	    std::uint32_t memorySize,
	    LexxExecutableHeader const& header,
	    ExternalProgramOptions const& options,
	    std::string_view path,
	    resources::Index const& resourceIndex) {
		// UI sessions, guest file slots and graphics state must not live
		// on the supervisor's relatively small FreeRTOS task stack.
#if defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_KERNEL_FREERTOS)
		// The Pico SDK heap can panic even on allocation through nothrow new.
		lexxos::program::ProgramMemoryArena vmArena;
		lexxos::program::ProgramRuntimeProfile runtime{};
		runtime.internalMemoryQuotaBytes = sizeof(Rv32iVirtualMachine);
		if (vmArena.reserve(runtime) != lexxos::program::ProgramMemoryArenaStatus::Ready) {
			return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
		}
		auto* const storage = vmArena.allocate(sizeof(Rv32iVirtualMachine), alignof(Rv32iVirtualMachine));
		if (storage == nullptr) {
			return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
		}
		auto* const instance = new (storage) Rv32iVirtualMachine{context, memory, memorySize, header, options, path, resourceIndex};
		auto const destroyVm = &std::destroy_at<Rv32iVirtualMachine>;
		std::unique_ptr<Rv32iVirtualMachine, decltype(destroyVm)> vm{instance, destroyVm};
#else
		std::unique_ptr<Rv32iVirtualMachine> vm{
		    new (std::nothrow) Rv32iVirtualMachine{
		                                           context, memory, memorySize, header, options, path, resourceIndex}
        };
#endif
		if (!vm) {
			return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
		}

		return vm->run();
	}

} // namespace

std::string_view toString(ExternalProgramStatus status) {
	switch (status) {
		case ExternalProgramStatus::Ok:
			return "ok";
		case ExternalProgramStatus::Cancelled:
			return "cancelled";
		case ExternalProgramStatus::OpenFailed:
			return "open-failed";
		case ExternalProgramStatus::ReadFailed:
			return "read-failed";
		case ExternalProgramStatus::ShortHeader:
			return "short-header";
		case ExternalProgramStatus::BadMagic:
			return "bad-magic";
		case ExternalProgramStatus::UnsupportedVersion:
			return "unsupported-version";
		case ExternalProgramStatus::UnsupportedMachine:
			return "unsupported-machine";
		case ExternalProgramStatus::UnsupportedAbi:
			return "unsupported-abi";
		case ExternalProgramStatus::UnsupportedCapabilities:
			return "unsupported-capabilities";
		case ExternalProgramStatus::InvalidHeader:
			return "invalid-header";
		case ExternalProgramStatus::InvalidImage:
			return "invalid-image";
		case ExternalProgramStatus::InvalidResources:
			return "invalid-resources";
		case ExternalProgramStatus::ImageTooLarge:
			return "image-too-large";
		case ExternalProgramStatus::ArgumentsTooLarge:
			return "arguments-too-large";
		case ExternalProgramStatus::MemoryUnavailable:
			return "memory-unavailable";
		case ExternalProgramStatus::InstructionLimit:
			return "instruction-limit";
		case ExternalProgramStatus::InstructionAddressFault:
			return "instruction-address-fault";
		case ExternalProgramStatus::LoadAddressFault:
			return "load-address-fault";
		case ExternalProgramStatus::StoreAddressFault:
			return "store-address-fault";
		case ExternalProgramStatus::IllegalInstruction:
			return "illegal-instruction";
		case ExternalProgramStatus::UnsupportedSyscall:
			return "unsupported-syscall";
	}

	return "unknown";
}

ExternalProgramResult runLexxExecutable(
    lexxos::os::AppContext& context,
    std::string_view path,
    ExternalProgramOptions const& options) {
	lexxos::vfs::FileHandle file{};
	auto const openStatus = context.vfs().open(path, lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok || !file.isOpen()) {
		return {.status = ExternalProgramStatus::OpenFailed, .exitCode = 126};
	}

	if (file.size() < lexxExecutableHeaderSize) {
		return {.status = ExternalProgramStatus::ShortHeader, .exitCode = 126};
	}

	std::uint8_t rawHeader[lexxExecutableHeaderSize]{};
	if (auto const status = readExact(file, rawHeader, sizeof(rawHeader)); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	LexxExecutableHeader header{};
	if (!parseHeader(rawHeader, sizeof(rawHeader), header)) {
		return {.status = ExternalProgramStatus::BadMagic, .exitCode = 126};
	}

	auto const reservedBytesAreZero = bytesAreZero(
	    rawHeader + lexxHeaderReservedOffset,
	    lexxExecutableHeaderSize - lexxHeaderReservedOffset);
	if (auto const status = validateHeader(header, file.size(), reservedBytesAreZero); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	resources::Index resourceIndex{};
	auto const imageEnd = static_cast<std::uint64_t>(header.imageOffset) + header.imageSize;
	if (resources::inspect(file, imageEnd, resourceIndex) == resources::InspectStatus::Invalid) {
		return {.status = ExternalProgramStatus::InvalidResources, .exitCode = 126};
	}

	// The .lexx header is validated before any guest memory is reserved.
	// This arena belongs exclusively to the supervisor's lexx-run task.
	// Never discard an existing live workspace from a different caller.
	auto* const arena = context.memoryArena();
	auto const heap = lexxos::kernel::memorySnapshot();
	auto const available = heap.available ? heap.freeHeapBytes : 0U;
	if (arena == nullptr || arena->active()) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126, .requiredMemoryBytes = header.memorySize, .availableMemoryBytes = available};
	}

	lexxos::program::ProgramRuntimeProfile runtime{};
	runtime.memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly;
	runtime.externalMemoryQuotaBytes = 0;
	runtime.internalMemoryQuotaBytes = header.memorySize;
	if (arena->reserve(runtime) != lexxos::program::ProgramMemoryArenaStatus::Ready) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126, .requiredMemoryBytes = header.memorySize, .availableMemoryBytes = available};
	}

	auto* memory = static_cast<std::uint8_t*>(arena->allocate(header.memorySize, 4));
	if (memory == nullptr) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126, .requiredMemoryBytes = header.memorySize, .availableMemoryBytes = available};
	}

	std::memset(memory, 0, header.memorySize);

	if (!file.seek(header.imageOffset)) {
		return {.status = ExternalProgramStatus::ReadFailed, .exitCode = 126};
	}

	if (auto const status = readExact(file, memory, header.imageSize); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	return runVirtualMachine(context, memory, header.memorySize, header, options, path, resourceIndex);
}

ExternalProgramResult benchmarkRv32i(
    lexxos::os::AppContext& context,
    Rv32iBenchmarkCase test,
    std::uint32_t iterations,
    std::uint8_t* scratch,
    std::size_t scratchSize) {
	constexpr std::uint32_t memoryBytes = 128;
	constexpr std::uint32_t counterAddress = 96;
	constexpr std::uint32_t dataAddress = 100;

	if (scratch == nullptr || scratchSize < memoryBytes) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
	}

	if (iterations == 0) {
		return {.status = ExternalProgramStatus::InvalidImage, .exitCode = 126};
	}

	// RV32I instruction encoders. Branch offsets are relative to
	// the address of the branch instruction, not the following PC.
	auto const encodeI = [](std::uint32_t opcode, std::uint32_t rd,
	                         std::uint32_t funct3, std::uint32_t rs1, std::int32_t imm) {
		return ((static_cast<std::uint32_t>(imm) & 0xFFFU) << 20U)
		    | (rs1 << 15U)
		    | (funct3 << 12U)
		    | (rd << 7U)
		    | opcode;
	};

	auto const encodeS = [](std::uint32_t funct3, std::uint32_t rs1,
	                         std::uint32_t rs2, std::int32_t imm) {
		auto const value = static_cast<std::uint32_t>(imm) & 0xFFFU;
		return ((value >> 5U) << 25U) | (rs2 << 20U) | (rs1 << 15U) | (funct3 << 12U) | ((value & 0x1FU) << 7U) | 0x23U;
	};

	auto const encodeB = [](std::uint32_t funct3, std::uint32_t rs1,
	                         std::uint32_t rs2, std::int32_t offset) {
		auto const value = static_cast<std::uint32_t>(offset);
		return (((value >> 12U) & 1U) << 31U)
		    | (((value >> 5U) & 0x3FU) << 25U)
		    | (rs2 << 20U)
		    | (rs1 << 15U)
		    | (funct3 << 12U)
		    | (((value >> 1U) & 0xFU) << 8U)
		    | (((value >> 11U) & 1U) << 7U)
		    | 0x63U;
	};

	std::array<std::uint32_t, 16> code{};
	std::uint32_t words = 0;
	auto emit = [&](std::uint32_t instruction) {
		code[words++] = instruction;
	};

	// x6 = iteration counter, x5 = checksum.
	emit(encodeI(0x03U, 6, 2, 0, counterAddress)); // lw x6, 96(x0)
	emit(encodeI(0x13U, 5, 0, 0, 0)); // addi x5, x0, 0

	switch (test) {
		case Rv32iBenchmarkCase::Alu: {
			auto const loop = words * 4U;
			emit(encodeI(0x13U, 5, 0, 5, 1)); // addi
			emit(encodeI(0x13U, 5, 4, 5, 0x155)); // xori
			emit(encodeI(0x13U, 5, 4, 5, 0x155)); // xori
			emit(encodeI(0x13U, 6, 0, 6, -1)); // decrement
			emit(encodeB(1, 6, 0,
			    static_cast<std::int32_t>(loop)
			        - static_cast<std::int32_t>(words * 4U)));
			break;
		}
		case Rv32iBenchmarkCase::Branch: {
			auto const loop = words * 4U;
			emit(encodeI(0x13U, 5, 0, 5, 1));
			emit(encodeB(0, 0, 0, 8)); // always taken
			emit(encodeI(0x13U, 5, 0, 5, 8)); // must be skipped
			emit(encodeI(0x13U, 6, 0, 6, -1));
			emit(encodeB(1, 6, 0,
			    static_cast<std::int32_t>(loop)
			        - static_cast<std::int32_t>(words * 4U)));
			break;
		}
		case Rv32iBenchmarkCase::Memory: {
			emit(encodeI(0x13U, 7, 0, 0, dataAddress));
			auto const loop = words * 4U;
			emit(encodeS(2, 7, 5, 0)); // sw x5, 0(x7)
			emit(encodeI(0x03U, 5, 2, 7, 0)); // lw x5, 0(x7)
			emit(encodeI(0x13U, 5, 0, 5, 1));
			emit(encodeI(0x13U, 6, 0, 6, -1));
			emit(encodeB(1, 6, 0,
			    static_cast<std::int32_t>(loop)
			        - static_cast<std::int32_t>(words * 4U)));
			break;
		}
		case Rv32iBenchmarkCase::ZeroRegister: {
			// A write to x0 must be discarded, not restored later.
			emit(encodeI(0x13U, 0, 0, 0, 5));
			auto const loop = words * 4U;
			emit(encodeI(0x13U, 5, 0, 5, 1));
			emit(encodeI(0x13U, 6, 0, 6, -1));
			emit(encodeB(1, 6, 0,
			    static_cast<std::int32_t>(loop)
			        - static_cast<std::int32_t>(words * 4U)));
			break;
		}
		case Rv32iBenchmarkCase::Ecall: {
			emit(encodeI(0x13U, 17, 0, 0, 6)); // a7 = argc
			auto const loop = words * 4U;
			emit(0x00000073U); // ecall
			emit(encodeI(0x13U, 6, 0, 6, -1));
			emit(encodeB(1, 6, 0,
			    static_cast<std::int32_t>(loop)
			        - static_cast<std::int32_t>(words * 4U)));
			break;
		}
	}

	// Return x5 for CPU/memory tests, zero for the syscall test.
	if (test == Rv32iBenchmarkCase::Ecall) {
		emit(encodeI(0x13U, 10, 0, 0, 0));
	} else {
		emit(encodeI(0x13U, 10, 0, 5, 0));
	}

	emit(encodeI(0x13U, 17, 0, 0, 0)); // a7 = exit
	emit(0x00000073U);

	std::memset(scratch, 0, memoryBytes);
	for (std::uint32_t index = 0; index < words; ++index) {
		writeLe32(scratch + index * 4U, code[index]);
	}

	writeLe32(scratch + counterAddress, iterations);

	LexxExecutableHeader header{};
	header.imageSize = words * 4U;
	header.memorySize = memoryBytes;
	header.entryOffset = 0;

	ExternalProgramOptions options{};
	resources::Index resources{};

	return runVirtualMachine(context, scratch, memoryBytes, header, options, {}, resources);
}

std::uint32_t benchmarkRv32iHost(
    Rv32iHostBenchmarkCase test, std::uint32_t iterations) {
	switch (test) {
		case Rv32iHostBenchmarkCase::RegisterAlu: {
			// No volatile stores in the loop: measure native
			// arithmetic without forcing per-iteration DDR traffic.
			std::uint32_t value = 0x12345678U;
			for (std::uint32_t index = 0; index < iterations; ++index) {
				value = value * 1664525U + 1013904223U;
				value ^= value >> 16U;
				value += index;
			}

			return value;
		}
		case Rv32iHostBenchmarkCase::DdrReadWrite: {
			// One volatile 32-bit read and write per iteration.
			// With D-cache disabled these accesses reach memory.
			std::uint32_t volatile words[32]{};
			std::uint32_t checksum = 0;
			for (std::uint32_t index = 0; index < iterations; ++index) {
				auto const slot = index & 31U;
				auto const value = words[slot];
				auto const next = value * 1664525U + 1013904223U + index;
				words[slot] = next;
				checksum ^= next;
			}

			return checksum;
		}
	}

	return 0;
}

Rv32iOpcodeResult benchmarkRv32iOpcode(
    lexxos::os::AppContext& context,
    Rv32iOpcodeProbe const& probe,
    std::uint32_t iterations,
    std::uint8_t* scratch,
    std::size_t scratchSize) {
	constexpr std::uint32_t memoryBytes = 128;
	constexpr std::uint32_t counterAddress = 96;
	constexpr std::uint32_t dataAddress = 100;

	if (scratch == nullptr || scratchSize < memoryBytes) {
		return {
		    .program = {
		                .status = ExternalProgramStatus::MemoryUnavailable,
		                .exitCode = 126,
		                },
		};
	}

	if (iterations == 0U) {
		return {
		    .program = {
		                .status = ExternalProgramStatus::InvalidImage,
		                .exitCode = 126,
		                },
		};
	}

	auto const encodeI = [](std::uint32_t op, std::uint32_t rd,
	                         std::uint32_t f3, std::uint32_t rs1, std::int32_t imm) {
		return ((static_cast<std::uint32_t>(imm) & 0xFFFU) << 20U)
		    | (rs1 << 15U)
		    | (f3 << 12U)
		    | (rd << 7U)
		    | op;
	};

	auto const encodeR = [](std::uint32_t f3,
	                         std::uint32_t f7, std::uint32_t rd,
	                         std::uint32_t rs1, std::uint32_t rs2) {
		return (f7 << 25U) | (rs2 << 20U) | (rs1 << 15U) | (f3 << 12U) | (rd << 7U) | 0x33U;
	};

	auto const encodeS = [](std::uint32_t f3,
	                         std::uint32_t rs1, std::uint32_t rs2, std::int32_t imm) {
		auto const u = static_cast<std::uint32_t>(imm) & 0xFFFU;
		return ((u >> 5U) << 25U) | (rs2 << 20U) | (rs1 << 15U) | (f3 << 12U) | ((u & 31U) << 7U) | 0x23U;
	};

	auto const encodeB = [](std::uint32_t f3,
	                         std::uint32_t rs1, std::uint32_t rs2, std::int32_t offset) {
		auto const u = static_cast<std::uint32_t>(offset);
		return (((u >> 12U) & 1U) << 31U)
		    | (((u >> 5U) & 63U) << 25U)
		    | (rs2 << 20U)
		    | (rs1 << 15U)
		    | (f3 << 12U)
		    | (((u >> 1U) & 15U) << 8U)
		    | (((u >> 11U) & 1U) << 7U)
		    | 0x63U;
	};

	std::array<std::uint32_t, 16> code{};
	std::uint32_t words = 0;
	auto emit = [&](std::uint32_t value) {
		code[words++] = value;
	};

	// x6: counter; x5: accumulated result.
	// x7/x8: operands; x9: operation result.
	emit(encodeI(0x03U, 6, 2, 0, counterAddress));
	emit(encodeI(0x13U, 5, 0, 0, 0));
	emit(encodeI(0x13U, 7, 0, 0, probe.lhs));
	emit(encodeI(0x13U, 8, 0, 0, probe.rhs));

	if (probe.form == Rv32iOpcodeForm::MixedSyscall) {
		emit(encodeI(0x13U, 17, 0, 0, 6)); // argc syscall
	}

	auto const loop = words * 4U;
	auto const bodyStart = words;
	std::uint32_t skippedInstructions = 0;

	switch (probe.form) {
		case Rv32iOpcodeForm::LoopControl:
			// No workload instruction: only the common decrement and BNE.
			break;
		case Rv32iOpcodeForm::Register:
			emit(encodeR(probe.funct3, probe.funct7, 9, 7, 8));
			emit(encodeR(0, 0, 5, 5, 9));
			break;

		case Rv32iOpcodeForm::Immediate:
			emit(encodeI(0x13U, 9, probe.funct3, 7,
			    probe.immediate));
			emit(encodeR(0, 0, 5, 5, 9));
			break;

		case Rv32iOpcodeForm::Load:
			emit(encodeI(0x03U, 9, probe.funct3, 7, 0));
			emit(encodeR(0, 0, 5, 5, 9));
			break;

		case Rv32iOpcodeForm::Store: {
			emit(encodeS(probe.funct3, 7, 8, 0));
			auto const loadType = probe.funct3 == 0U ? 4U
			    : probe.funct3 == 1U                 ? 5U
			                                         : 2U;
			emit(encodeI(0x03U, 9, loadType, 7, 0));
			emit(encodeR(0, 0, 5, 5, 9));
			break;
		}

		case Rv32iOpcodeForm::Branch:
			emit(encodeB(probe.funct3, 7, 8, 8));
			emit(encodeI(0x13U, 5, 0, 5, 1));
			emit(encodeI(0x13U, 5, 0, 5, 1));
			// contribution=1: taken, skips first ADDI.
			skippedInstructions = probe.contribution == 1 ? 1U : 0U;
			break;

		case Rv32iOpcodeForm::Jump:
			emit(0x0080006FU); // jal x0, +8
			emit(encodeI(0x13U, 5, 0, 5, 99)); // skipped
			emit(encodeI(0x13U, 5, 0, 5, 1));
			skippedInstructions = 1;
			break;

		case Rv32iOpcodeForm::JumpRegister:
			emit(encodeI(0x67U, 0, 0, 7, 0));
			emit(encodeI(0x13U, 5, 0, 5, 99)); // skipped
			emit(encodeI(0x13U, 5, 0, 5, 1));
			skippedInstructions = 1;
			break;

		case Rv32iOpcodeForm::LoadUpper:
			emit(0x000014B7U); // lui x9, 0x1
			emit(encodeI(0x13U, 9, 5, 9, 12)); // srli x9,12
			emit(encodeR(0, 0, 5, 5, 9));
			break;

		case Rv32iOpcodeForm::AddUpperPc:
			emit(0x00000497U); // auipc x9, 0
			emit(encodeI(0x13U, 9, 0, 9,
			    -static_cast<std::int32_t>(loop)));
			emit(encodeI(0x13U, 9, 0, 9, 1));
			emit(encodeR(0, 0, 5, 5, 9));
			break;

		case Rv32iOpcodeForm::Fence:
			emit(0x0000000FU);
			emit(encodeI(0x13U, 5, 0, 5, 1));
			break;

		case Rv32iOpcodeForm::MixedAluMemory:
			emit(encodeI(0x03U, 9, 2, 7, 0));
			emit(encodeI(0x13U, 9, 0, 9, 1));
			emit(encodeS(2, 7, 9, 0));
			emit(encodeI(0x13U, 5, 0, 5, 1));
			break;

		case Rv32iOpcodeForm::MixedBranchMemory:
			emit(encodeI(0x03U, 9, 2, 7, 0));
			emit(encodeB(0, 9, 8, 8)); // beq
			emit(encodeI(0x13U, 5, 0, 5, 99)); // skipped
			emit(encodeI(0x13U, 5, 0, 5, 1));
			emit(encodeS(2, 7, 8, 0));
			skippedInstructions = 1;
			break;

		case Rv32iOpcodeForm::MixedSyscall:
			emit(0x00000073U);
			emit(encodeI(0x13U, 5, 0, 5, 1));
			break;
	}

	emit(encodeI(0x13U, 6, 0, 6, -1));
	emit(encodeB(1, 6, 0,
	    static_cast<std::int32_t>(loop)
	        - static_cast<std::int32_t>(words * 4U)));

	auto const perIteration =
	    words - bodyStart - skippedInstructions;

	emit(encodeI(0x13U, 10, 0, 5, 0));
	emit(encodeI(0x13U, 17, 0, 0, 0));
	emit(0x00000073U);

	// auto const fixedInstructions = words - (words - bodyStart) - 3U + 3U;
	auto const fixedInstructions = bodyStart + 3U;
	// Equivalent to setup words + 3 exit instructions.
	// The loop's decrement and branch belong to perIteration.

	std::memset(scratch, 0, memoryBytes);
	for (std::uint32_t i = 0; i < words; ++i) {
		writeLe32(scratch + i * 4U, code[i]);
	}

	writeLe32(scratch + counterAddress, iterations);

	if (probe.form == Rv32iOpcodeForm::Load) {
		writeLe32(scratch + dataAddress,
		    static_cast<std::uint32_t>(probe.immediate));
	} else if (probe.form == Rv32iOpcodeForm::MixedBranchMemory) {
		writeLe32(scratch + dataAddress, 1U);
	}

	LexxExecutableHeader header{};
	header.imageSize = words * 4U;
	header.memorySize = memoryBytes;
	header.entryOffset = 0;

	ExternalProgramOptions options{};
	resources::Index resources{};

	auto const expected = static_cast<std::uint32_t>(
	    static_cast<std::int64_t>(probe.contribution) * iterations);

	Rv32iOpcodeResult output{};
	output.instructionsPerIteration = perIteration;
	output.fixedInstructions = fixedInstructions;
	output.expectedExit =
	    static_cast<int>(static_cast<std::int32_t>(expected));
	output.program = runVirtualMachine(context, scratch, memoryBytes, header, options, {}, resources);

	return output;
}

} // namespace lexxos::external_program
