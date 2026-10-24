#include <lexxos/apps/gameboy/gb_emu_program.h>

#include <lexxos/apps/gameboy/detail/gameboy_launcher.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/feature.h>
#include <lexxos/vfs/vfs.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::anyProgramCore;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramInterfaceKind;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM) || defined(LEXXOS_PLATFORM_ESPIDF)
	constexpr bool gbEmuHeapRuntimeEnabled = true;
#else
	constexpr bool gbEmuHeapRuntimeEnabled = false;
#endif
	constexpr ProgramMemoryClass gbEmuMemoryClass =
	    gbEmuHeapRuntimeEnabled ? ProgramMemoryClass::ExternalPreferred : ProgramMemoryClass::ExternalRequired;
	constexpr std::uint32_t gbEmuExternalMemoryQuotaBytes = gbEmuHeapRuntimeEnabled ? 0U : 1024U * 1024U;
#if defined(LEXXOS_PLATFORM_PICO) && defined(FREE_RTOS_KERNEL_SMP) && FREE_RTOS_KERNEL_SMP && defined(LEXXOS_PICO_FREERTOS_CORES) && LEXXOS_PICO_FREERTOS_CORES > 1
	constexpr int gbEmuCoreAffinity = 1;
#else
	constexpr int gbEmuCoreAffinity = anyProgramCore;
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF)
	constexpr std::uint32_t gbEmuStackSizeBytes = 20U * 1024U;
#else
	constexpr std::uint32_t gbEmuStackSizeBytes = 12U * 1024U;
#endif

	bool appendPathPart(std::array<char, lexxos::vfs::maxPathLength>& path, std::size_t& size, std::string_view text) {
		if (size + text.size() >= path.size()) {
			return false;
		}
		if (!text.empty()) {
			std::memcpy(path.data() + size, text.data(), text.size());
			size += text.size();
		}
		path[size] = '\0';
		return true;
	}

	bool copyGbEmuNamedArguments(lexxos::program::ParsedArguments const& source, lexxos::program::ParsedArguments& target) {
		for (auto const& argument : gbEmuArguments) {
			if (argument.kind == ProgramArgumentKind::Positional) {
				continue;
			}
			if (source.has(argument.name)) {
				if (!target.setNamed(argument.name, source.value(argument.name), true)) {
					return false;
				}
				continue;
			}
			auto const value = source.value(argument.name);
			if (!value.empty() && !target.setNamed(argument.name, value, false)) {
				return false;
			}
		}
		return true;
	}

	bool looksLikeOption(std::string_view token) {
		return token.size() > 1 && token[0] == '-';
	}

	int runGameBoyEmulatorFromCli(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments) {
		if (arguments.positionalCount() <= 1) {
			return runGameBoyEmulator(context, arguments);
		}

		std::array<char, lexxos::vfs::maxPathLength> romPath{};
		std::size_t romPathSize = 0;
		for (std::size_t index = 0; index < arguments.positionalCount(); ++index) {
			auto const token = arguments.positional(index);
			if (index != 0 && looksLikeOption(token)) {
				context.writeError("gbemu: unexpected argument: ");
				context.writeErrorLine(token);
				return 2;
			}
			if (index != 0 && !appendPathPart(romPath, romPathSize, " ")) {
				context.writeErrorLine("gbemu: ROM path is too long");
				return 2;
			}
			if (!appendPathPart(romPath, romPathSize, token)) {
				context.writeErrorLine("gbemu: ROM path is too long");
				return 2;
			}
		}

		lexxos::program::ParsedArguments normalized;
		if (!copyGbEmuNamedArguments(arguments, normalized) || !normalized.addPositional("rom", romPath.data())) {
			context.writeErrorLine("gbemu: too many parsed arguments");
			return 2;
		}
		return runGameBoyEmulator(context, normalized);
	}

} // namespace

ProgramDescriptor GbEmuProgram::descriptor() {
	return {
	    .name = "gbemu",
	    .help = "run Game Boy ROMs or open the ROM launcher",
	    .description = "Without arguments opens a TUI ROM launcher. With a ROM path, loads a Game Boy ROM, runs a bounded CPU/bus/timer/joypad loop, renders the current background and prints compact runtime diagnostics.",
	    .usage = "gbemu [--frames count] [--steps-per-frame count] [--cycles-per-frame count] [--real-frame] [--until-vram] [--until-visible] [--until-tile-data] [--until-bg-ready] [--min-tile-data count] [--max-cycles count] [--force-visible] [--bg-map auto|9800|9c00] [--bg-tiles auto|8000|8800] [--display crop|fit|none] [--palette name] [--render-every count] [--input-every count] [--preset none|playable] [--playable-fast] [--max-speed] [--psram-fast] [--rom-flash] [--rom-flash-reuse] [--rom-psram] [--rom-psram-reuse] [--audio] [--no-audio] [--no-ppu] [--fast-timing] [--sync-realtime] [--profile] [--profile-every count] [--no-fast-halt] [--break-pc address] [--break-pc-from address] [--watch-blank] [--watch-blank-after count] [--watch-blank-streak count] [--break-bgp-zero-after count] [--fault-trace] [--trace] [rom]",
	    .arguments = argumentList(gbEmuArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = gbEmuStackSizeBytes,
	                .priority = 2,
	                .memoryClass = gbEmuMemoryClass,
	                .externalMemoryQuotaBytes = gbEmuExternalMemoryQuotaBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                .interfaceKind = ProgramInterfaceKind::Graphical,
	                .coreAffinity = gbEmuCoreAffinity,
	                },
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = FEATURE_ENABLED(PSRAM) || gbEmuHeapRuntimeEnabled,
	};
}

int GbEmuProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	if (arguments.positional("rom").empty()) {
		return runGameBoyLauncher(context);
	}
	return runGameBoyEmulatorFromCli(context, arguments);
}

} // namespace lexxos::apps::gameboy_programs
