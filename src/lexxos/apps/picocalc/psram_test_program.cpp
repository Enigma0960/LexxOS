#include <lexxos/apps/picocalc/psram_test_program.h>

#include <cstddef>
#include <cstdint>
#include <new>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform.h>
#include <lexxos/program/program_memory_arena.h>

namespace lexxos::apps::picocalc_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	constexpr std::uint32_t psramTestWorkspaceBytes =
	    static_cast<std::uint32_t>((sizeof(detail::PsramTestBuffer) + alignof(detail::PsramTestBuffer)) * 3U);
	constexpr std::uint32_t psramMaxTestPasses = 1000;

	constexpr lexxos::program::ProgramArgumentSpec psramTestArguments[] = {
	    {
         .name = "address",
         .shortName = 'a',
         .valueName = "addr",
         .help = "24-bit start address, decimal or 0xhex",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "length",
         .shortName = 'l',
         .valueName = "bytes",
         .help = "bytes to test, maximum 4096",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "256",
	     },
	    {
         .name = "passes",
         .shortName = 'p',
         .valueName = "count",
         .help = "number of write/read/verify passes, maximum 1000",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "1",
	     },
	};

	bool allocateBuffer(
	    lexxos::os::AppContext& context,
	    detail::PsramTestBuffer*& buffer) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("psram-test: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(detail::PsramTestBuffer), alignof(detail::PsramTestBuffer));
		if (memory == nullptr) {
			context.writeLine("psram-test: not enough program workspace memory");
			return false;
		}

		buffer = new (memory) detail::PsramTestBuffer{};
		return true;
	}

	bool allocateWorkspace(
	    lexxos::os::AppContext& context,
	    detail::PsramTestBuffer*& original,
	    detail::PsramTestBuffer*& expected,
	    detail::PsramTestBuffer*& actual) {
		return allocateBuffer(context, original) && allocateBuffer(context, expected) && allocateBuffer(context, actual);
	}

} // namespace

ProgramDescriptor PsramTestProgram::descriptor() {
	return {
	    .name = "psram-test",
	    .help = "test external PSRAM read/write",
	    .description = "Reads, writes, verifies and restores a PSRAM block using the board PSRAM burst-transfer path.",
	    .usage = "psram-test [--address <addr>] [--length <bytes>] [--passes <count>]",
	    .arguments = argumentList(psramTestArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = psramTestWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int PsramTestProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const& board = activeBoard(context);
	if (!board.psram.spi.isValid()) {
		context.writeLine("psram-test: PSRAM SPI wiring is not configured");
		return 1;
	}

	std::uint32_t address = detail::psramDefaultTestAddress;
	std::uint32_t length = detail::psramDefaultTestLength;
	if (!detail::parseOptionalUnsignedFlexible(arguments, "address", address)
	    || address >= detail::psramAddressSpaceBytes) {
		context.writeErrorLine("psram-test: invalid --address");
		return 2;
	}
	if (!detail::parseOptionalUnsignedFlexible(arguments, "length", length)
	    || length == 0
	    || length > detail::psramMaxTestLength
	    || length > detail::psramAddressSpaceBytes - address) {
		context.writeErrorLine("psram-test: invalid --length");
		return 2;
	}

	std::uint32_t passes = 1;
	if (!detail::parseOptionalUnsignedFlexible(arguments, "passes", passes)
	    || passes == 0
	    || passes > psramMaxTestPasses) {
		context.writeErrorLine("psram-test: invalid --passes");
		return 2;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	auto const byteLength = static_cast<std::size_t>(length);
	detail::PsramTestBuffer* original = nullptr;
	detail::PsramTestBuffer* expected = nullptr;
	detail::PsramTestBuffer* actual = nullptr;
	if (!allocateWorkspace(context, original, expected, actual)) {
		return 1;
	}
	bool restoreRequired = false;

	context.write("psram-test: address=");
	detail::writeHexAddress(context, address);
	context.write(" length=");
	writeUnsigned(context, length);
	context.write(" passes=");
	writeUnsigned(context, passes);
	context.writeLine("");

	if (!detail::readPsramMemory(board.psram, address, original->data(), byteLength)) {
		context.writeLine("psram-test: initial read failed");
		return 1;
	}

	auto restoreOriginal = [&]() {
		if (!restoreRequired) {
			return true;
		}
		if (!detail::writePsramMemory(board.psram, address, original->data(), byteLength)) {
			context.writeLine("psram-test: restore write failed");
			return false;
		}
		if (!detail::readPsramMemory(board.psram, address, actual->data(), byteLength)) {
			context.writeLine("psram-test: restore read failed");
			return false;
		}
		std::size_t mismatchOffset = 0;
		if (!detail::verifyPsramBuffer(*original, *actual, byteLength, mismatchOffset)) {
			context.write("psram-test: restore mismatch offset=");
			writeUnsigned(context, static_cast<std::uint32_t>(mismatchOffset));
			context.write(" expected=");
			writeHexByte(context, (*original)[mismatchOffset]);
			context.write(" actual=");
			writeHexByte(context, (*actual)[mismatchOffset]);
			context.writeLine("");
			return false;
		}
		restoreRequired = false;
		return true;
	};

	auto runPattern = [&](std::uint32_t pass, std::uint8_t seed) {
		detail::fillPsramPattern(*expected, address, byteLength, seed);
		restoreRequired = true;
		if (!detail::writePsramMemory(board.psram, address, expected->data(), byteLength)) {
			context.writeLine("psram-test: pattern write failed");
			return false;
		}
		if (!detail::readPsramMemory(board.psram, address, actual->data(), byteLength)) {
			context.writeLine("psram-test: pattern read failed");
			return false;
		}
		std::size_t mismatchOffset = 0;
		if (!detail::verifyPsramBuffer(*expected, *actual, byteLength, mismatchOffset)) {
			context.write("psram-test: mismatch seed=");
			writeHexByte(context, seed);
			context.write(" pass=");
			writeUnsigned(context, pass + 1U);
			context.write(" offset=");
			writeUnsigned(context, static_cast<std::uint32_t>(mismatchOffset));
			context.write(" expected=");
			writeHexByte(context, (*expected)[mismatchOffset]);
			context.write(" actual=");
			writeHexByte(context, (*actual)[mismatchOffset]);
			context.writeLine("");
			return false;
		}
		return true;
	};

	bool ok = true;
	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		auto const firstSeed = static_cast<std::uint8_t>(0xA5U ^ ((pass * 37U) & 0xFFU));
		auto const secondSeed = static_cast<std::uint8_t>(0x5AU ^ ((pass * 53U) & 0xFFU));
		if (!runPattern(pass, firstSeed) || !runPattern(pass, secondSeed)) {
			ok = false;
			break;
		}
	}

	bool const restored = restoreOriginal();
	if (!ok || !restored) {
		context.write("psram-test: failed restored=");
		context.write(restored ? "yes" : "no");
		context.writeLine("");
		return 1;
	}

	lexxos::platform::GameBoyRomPsramRuntimeInfo runtime{};
	if (lexxos::platform::gameBoyRomPsramRuntimeInfo(runtime)) {
		context.write("psram-test: backend=");
		context.write(runtime.backend == nullptr ? "none" : runtime.backend);
		context.write(" effective-baud=");
		writeUnsigned(context, runtime.effectiveBaudRate);
		context.write("Hz pio-transfers=");
		writeUnsigned(context, runtime.pioTransferCount);
		context.write(" software-transfers=");
		writeUnsigned(context, runtime.softwareTransferCount);
		context.write(" last-pio-failure=");
		context.write(runtime.lastPioFailure == nullptr ? "none" : runtime.lastPioFailure);
		context.writeLine("");
	}

	context.write("psram-test: ok restored=yes passes=");
	writeUnsigned(context, passes);
	context.write(" patterns=");
	writeUnsigned(context, passes * 2U);
	context.writeLine("");

	return 0;
#else
	context.writeLine("psram-test: SPI probe is unavailable on this platform");
	return 1;
#endif
}

} // namespace lexxos::apps::picocalc_programs
