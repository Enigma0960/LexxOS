#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::gameboy_programs {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;

constexpr ProgramArgumentSpec romPathArguments[] = {
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbRomCheckArguments[] = {
    {
     .name = "passes",
     .shortName = 'p',
     .valueName = "count",
     .help = "number of repeated file-read passes",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "2",
     },
    {
     .name = "full",
     .help = "also hash the full ROM image; slow on FAT/SD because it walks large files",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbCartArguments[] = {
    {
     .name = "bank",
     .shortName = 'b',
     .valueName = "number",
     .help = "switchable ROM bank to select for the mapper read test",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "2",
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbBusArguments[] = {
    {
     .name = "bank",
     .shortName = 'b',
     .valueName = "number",
     .help = "switchable ROM bank to select through the bus mapper control registers",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "2",
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbCpuStepArguments[] = {
    {
     .name = "steps",
     .shortName = 's',
     .valueName = "count",
     .help = "number of CPU instructions to execute",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "8",
     },
    {
     .name = "pc",
     .valueName = "address",
     .help = "override initial program counter, decimal or 0x-prefixed hexadecimal",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "0x0100",
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};
constexpr ProgramArgumentSpec gbVramViewArguments[] = {
    {
     .name = "tile",
     .shortName = 't',
     .valueName = "number",
     .help = "first tile index to show from VRAM tile data",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "0",
     },
    {
     .name = "mode",
     .shortName = 'm',
     .valueName = "tiles|map|map-tiles|bg|bg-scan|heat",
     .help = "VRAM visualization mode: tile data, BG tile map, synthetic map tiles, forced BG render, BG mode scan, or raw VRAM heatmap",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "tiles",
     },
    {
     .name = "target",
     .valueName = "auto|any|tile-data|map|map0|map1",
     .help = "VRAM area that must reach min-target before rendering",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "min-target",
     .valueName = "count",
     .help = "minimum non-zero bytes in the selected VRAM target before rendering",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "1",
     },
    {
     .name = "auto-tile",
     .help = "for tile mode, start the view near the first non-zero tile-data byte",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "force-visible",
     .help = "for BG mode, force LCD enable, BG enable and DMG palette for diagnostics",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "bg-map",
     .valueName = "auto|9800|9c00",
     .help = "for BG diagnostics, override the background tile-map base",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "bg-tiles",
     .valueName = "auto|8000|8800",
     .help = "for BG diagnostics, override unsigned or signed tile-data addressing",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "max-cycles",
     .valueName = "count",
     .help = "maximum warmup cycles before rendering VRAM data",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "2247168",
     },
    {
     .name = "trace",
     .shortName = 'r',
     .help = "print warmup CPU and IO diagnostics",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "display",
     .valueName = "crop|fit|none",
     .help = "display mapping: crop keeps 1:1 pixels, fit downsamples the full 160x144 GB frame, none runs without blitting",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "crop",
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbTestRunArguments[] = {
    {
     .name = "cycles",
     .shortName = 'c',
     .valueName = "count",
     .help = "maximum emulated CPU cycles before stopping the test ROM",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "2247168",
     },
    {
     .name = "trace",
     .shortName = 't',
     .help = "print final CPU stop diagnostics",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy test ROM image path",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbTestSuiteArguments[] = {
    {
     .name = "cycles",
     .shortName = 'c',
     .valueName = "count",
     .help = "maximum emulated CPU cycles per test ROM",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "143818752",
     },
    {
     .name = "trace",
     .shortName = 't',
     .help = "print detailed diagnostics for every test ROM",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "stop-on-fail",
     .help = "stop the suite after the first failed, incomplete or stopped ROM",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "dir",
     .valueName = "path",
     .help = "directory containing Blargg cpu_instrs/individual ROM files",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = true,
     },
};

constexpr ProgramArgumentSpec gbEmuArguments[] = {
    {
     .name = "audio",
     .help = "enable Game Boy APU PCM playback on targets where it is disabled by default",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "no-audio",
     .help = "disable Game Boy APU PCM playback even when a PCM audio device is available",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "frames",
     .shortName = 'f',
     .valueName = "count",
     .help = "number of emulated frames to run before returning to shell; 0 runs until cancel/exit combo; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "steps-per-frame",
     .shortName = 's',
     .valueName = "count",
     .help = "maximum CPU instructions to execute before each rendered frame; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "cycles-per-frame",
     .shortName = 'c',
     .valueName = "count",
     .help = "target emulated CPU cycles per rendered frame; 0 keeps steps-per-frame mode; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "trace",
     .shortName = 't',
     .help = "print one compact runtime line after each rendered frame",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "fault-trace",
     .help = "debug: keep a rolling opcode trace and dump it when gbemu detects a suspicious state",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "watch-blank",
     .help = "debug: stop and dump CPU/PPU state when a rendered frame becomes fully white after visible frames",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "watch-blank-after",
     .valueName = "count",
     .help = "debug: ignore blank-frame detection before the given emulated frame",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "0",
     },
    {
     .name = "watch-blank-streak",
     .valueName = "count",
     .help = "debug: require this many rendered blank frames before stopping",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "1",
     },
    {
     .name = "break-bgp-zero-after",
     .valueName = "count",
     .help = "debug: stop when BGP becomes 0 after the given emulated frame",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     .defaultValue = "0",
     },
    {
     .name = "profile",
     .shortName = 'p',
     .help = "debug: print timing breakdown for gbemu stages without per-frame opcode trace",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "profile-every",
     .valueName = "count",
     .help = "debug: print one profile line after every N emulated frames; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "real-frame",
     .help = "use the original DMG frame budget (70224 cycles) when cycles-per-frame is 0",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "sync-realtime",
     .help = "limit playback to the original Game Boy realtime speed when cycle-based frame timing is enabled",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "until-vram",
     .help = "fast-forward before the first render until the ROM writes to VRAM",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "until-visible",
     .help = "fast-forward before the first render until any VRAM byte is non-zero",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "until-tile-data",
     .help = "fast-forward before the first render until VRAM tile data 0x8000-0x97FF is non-zero",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "until-bg-ready",
     .help = "fast-forward before the first render until BG tile map and tile data are populated",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "min-tile-data",
     .valueName = "count",
     .help = "minimum non-zero tile-data bytes used by --until-bg-ready; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "max-cycles",
     .valueName = "count",
     .help = "maximum fast-forward cycles used by warmup options; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "force-visible",
     .help = "debug: force LCD enable, BG enable and DMG palette before rendering",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "bg-map",
     .valueName = "auto|9800|9c00",
     .help = "debug: override background tile-map base before rendering",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "bg-tiles",
     .valueName = "auto|8000|8800",
     .help = "debug: override background tile-data addressing before rendering",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "display",
     .valueName = "crop|fit|1x|none",
     .help = "display mapping: crop/fit scale to the LCD, 1x centers the native 160x144 frame, none runs without blitting; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "palette",
     .valueName = "name",
     .help = "RGB565 palette: default, dmg_green, pocket_gray, gbc_blue, gbc_brown, gbc_red, gbc_pastel or high_contrast; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "render-every",
     .valueName = "count",
     .help = "blit only every N emulated frames and always blit the final frame; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "input-every",
     .valueName = "count",
     .help = "poll gamepad only every N emulated frames; skipped frames reuse the previous state; default comes from settings.ini",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "preset",
     .valueName = "none|playable",
     .help = "apply a known playback preset for the current run; settings.ini speed_mode provides the default launcher/direct-run mode",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "",
     },
    {
     .name = "playable-fast",
     .help = "shortcut for visual playback: real DMG frame budget and render every 2nd frame unless overridden",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "max-speed",
     .help = "PicoCalc performance shortcut: real-frame, render every 2nd frame, and reuse/program XIP flash ROM staging unless ROM storage is explicitly selected",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "psram-fast",
     .help = "PicoCalc safe performance shortcut: real-frame, render every 2nd frame, and reuse/copy ROM through PSRAM unless ROM storage is explicitly selected",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom-flash",
     .help = "copy ROM to the reserved RP2xxx XIP flash staging area before emulation; removes SD/FAT reads from the hot path",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom-flash-reuse",
     .help = "reuse the reserved RP2xxx XIP flash staging area when it already contains the same ROM; otherwise program it",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom-psram",
     .help = "copy ROM to PicoCalc PSRAM before emulation; cache misses are served from PSRAM instead of SD/FAT",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom-psram-reuse",
     .help = "reuse PicoCalc PSRAM when it already contains the same ROM; otherwise copy it from SD/FAT",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "no-ppu",
     .help = "debug: skip framebuffer rendering entirely; useful for pure CPU/bus profiling",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "fast-timing",
     .help = "experimental: use VBlank-only lightweight PPU timing during gbemu playback",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "break-pc",
     .valueName = "address",
     .help = "debug: stop the current frame as soon as CPU reaches the given PC, decimal or 0x-prefixed",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "",
     },
    {
     .name = "break-pc-from",
     .valueName = "address",
     .help = "debug: stop when CPU PC becomes greater than or equal to the given address; useful for catching jumps into RAM/HRAM",
     .kind = ProgramArgumentKind::Option,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "",
     },
    {
     .name = "no-fast-halt",
     .help = "debug: disable HALT fast-forwarding used by gbemu",
     .kind = ProgramArgumentKind::Flag,
     .valueType = ProgramArgumentValueType::String,
     .required = false,
     },
    {
     .name = "rom",
     .valueName = "path",
     .help = "Game Boy ROM image path; without a ROM gbemu opens the launcher",
     .kind = ProgramArgumentKind::Positional,
     .valueType = ProgramArgumentValueType::Path,
     .required = false,
     },
};

constexpr int gameBoyWidth = 160;
constexpr int gameBoyHeight = 144;
constexpr int maxSkeletonScale = 2;
constexpr int maxSkeletonLinePixels = gameBoyWidth * maxSkeletonScale;
constexpr std::uint16_t skeletonBorderColor = 0x18E3;
constexpr std::uint16_t skeletonDarkColor = 0x2104;
constexpr std::uint16_t skeletonShade0 = 0xFFFF;
constexpr std::uint16_t skeletonShade1 = 0xBDF7;
constexpr std::uint16_t skeletonShade2 = 0x7BEF;
constexpr std::uint16_t skeletonShade3 = 0x39E7;
constexpr std::uint16_t gbDisplayBackgroundColor = 0x2104;
constexpr std::uint32_t defaultGbEmuStepsPerFrame = 512;
constexpr std::uint32_t maxGbEmuStepsPerFrame = 32768;
constexpr std::uint32_t defaultGbCyclesPerFrame = 70224;
constexpr std::uint32_t dmgCpuClockHz = 4194304;
constexpr std::uint32_t maxGbSyncSleepChunkMs = 20;
constexpr std::uint32_t maxGbCyclesPerFrame = 70224 * 4;
constexpr std::uint32_t maxGbEmuFrames = 16384;
constexpr std::uint32_t defaultGbEmuProfileEvery = 60;
constexpr std::uint32_t fastHaltStepCycles = 252;
constexpr std::uint32_t defaultGbEmuWarmupCycles = defaultGbCyclesPerFrame * 128U;
constexpr std::uint32_t maxGbEmuWarmupCycles = defaultGbCyclesPerFrame * 512U;
constexpr std::uint32_t defaultGbTestCycles = defaultGbCyclesPerFrame * 32U;
constexpr std::uint32_t defaultGbTestSuiteCycles = defaultGbCyclesPerFrame * 2048U;
constexpr std::uint32_t maxGbTestCycles = defaultGbCyclesPerFrame * 8192U;
constexpr std::size_t gameBoyWorkspaceHeapGuardBytes = 2048U;

constexpr std::string_view blarggCpuIndividualRoms[] = {
    "01-special.gb",
    "02-interrupts.gb",
    "03-op sp,hl.gb",
    "04-op r,imm.gb",
    "05-op rp.gb",
    "06-ld r,r.gb",
    "07-jr,jp,call,ret,rst.gb",
    "08-misc instrs.gb",
    "09-op r,r.gb",
    "10-bit ops.gb",
    "11-op a,(hl).gb",
};

} // namespace lexxos::apps::gameboy_programs
