#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/gameboy/detail/cartridge_lifetime_guard.h>
#include <lexxos/apps/gameboy/detail/gameboy_diagnostic_types.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_types.h>
#include <lexxos/apps/gameboy/detail/gameboy_runtime_types.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cartridge.h>
#include <lexxos/gameboy/cartridge_header.h>
#include <lexxos/gameboy/cpu.h>
#include <lexxos/gameboy/ppu.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform.h>
#include <lexxos/program/program.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::gameboy_programs {

using detail::BackgroundUsageStats;
using detail::CartridgeLifetimeGuard;
using detail::GameBoyWorkspace;
using detail::GbEmuProfileWindow;
using detail::GbTestRunSummary;
using detail::RunBudgetResult;
using detail::RunTraceBuffer;
using detail::SpriteUsageStats;
using detail::VramFirstNonZero;

GameBoyWorkspace* gameBoyWorkspace(lexxos::os::AppContext& context);
int runGameBoyEmulator(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments);

std::string_view toString(VramViewMode mode);
bool parseVramViewMode(std::string_view value, VramViewMode& mode);
bool parseVramWarmupTarget(std::string_view value, VramWarmupTarget& target);
bool parseBackgroundMapMode(std::string_view value, lexxos::gameboy::BackgroundMapMode& mode);
bool parseBackgroundTileDataMode(std::string_view value, lexxos::gameboy::BackgroundTileDataMode& mode);
std::string_view toString(lexxos::gameboy::BackgroundMapMode mode);
std::string_view toString(lexxos::gameboy::BackgroundTileDataMode mode);
bool parseDisplayBlitMode(std::string_view value, lexxos::gameboy::DisplayBlitMode& mode);
std::string_view toString(lexxos::gameboy::DisplayBlitMode mode);
bool parseRgb565Palette(std::string_view value, lexxos::gameboy::Rgb565Palette& palette);
std::string_view rgb565PaletteNames();
VramWarmupTarget effectiveWarmupTarget(VramViewMode mode, VramWarmupTarget target);
std::string_view toString(VramWarmupTarget target);

lexxos::gameboy::JoypadState toJoypadState(lexxos::devices::GamepadState const& state);
bool isContinuableEmulationStatus(lexxos::gameboy::CpuStepStatus status);
std::uint32_t elapsedMs(std::uint32_t startMs, std::uint32_t endMs);
std::uint32_t profileOtherMs(GbEmuProfileWindow const& profile);
void writeGbEmuProfileLine(
    lexxos::os::AppContext& context,
    char const* label,
    std::uint32_t lastFrame,
    GbEmuProfileWindow const& profile,
    std::uint32_t cyclesPerFrame);

RunBudgetResult runCpuBudget(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t stepLimit,
    std::uint32_t cycleLimit,
    RunTraceBuffer* trace = nullptr,
    lexxos::gameboy::Cartridge const* cartridge = nullptr,
    std::uint16_t const* breakPc = nullptr,
    std::uint16_t const* breakPcFrom = nullptr,
    bool fastHalt = false,
    bool fastTiming = false);
RunBudgetResult runUntilVramWrite(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    RunTraceBuffer* trace = nullptr,
    lexxos::gameboy::Cartridge const* cartridge = nullptr,
    std::uint16_t const* breakPc = nullptr,
    bool fastHalt = false);
RunBudgetResult runUntilVramTarget(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    VramWarmupTarget target,
    std::uint32_t minTarget = 1,
    RunTraceBuffer* trace = nullptr,
    lexxos::gameboy::Cartridge const* cartridge = nullptr,
    std::uint16_t const* breakPc = nullptr,
    bool fastHalt = false);
RunBudgetResult runUntilBgReady(
    lexxos::gameboy::Cpu& cpu,
    lexxos::gameboy::Bus& bus,
    std::uint32_t cycleLimit,
    std::uint32_t minTileData,
    RunTraceBuffer* trace = nullptr,
    lexxos::gameboy::Cartridge const* cartridge = nullptr,
    std::uint16_t const* breakPc = nullptr,
    bool fastHalt = false);

void writeHex16(lexxos::os::AppContext& context, std::uint16_t value);
void writeCode8(lexxos::os::AppContext& context, std::uint8_t value);
void writeNamedCode(lexxos::os::AppContext& context, std::string_view name, std::uint8_t code);
void writeBytesField(lexxos::os::AppContext& context, std::uint64_t bytes);
bool parseUnsigned(std::string_view value, std::uint32_t& output);
bool parseNumber(std::string_view value, std::uint32_t& output);

bool readCartridgeHeader(
    lexxos::os::AppContext& context,
    std::string_view path,
    std::array<std::uint8_t, lexxos::gameboy::cartridgeHeaderSize>& header,
    lexxos::vfs::FileHandle& file);
void writeRomSizeConsistency(lexxos::os::AppContext& context, std::uint64_t actualSize, std::uint32_t expectedSize);
void writeCartridgeFeatures(lexxos::os::AppContext& context, lexxos::gameboy::CartridgeFeatures const& features);
void writeReadByte(lexxos::os::AppContext& context, std::string_view label, std::uint8_t value);
void writeBusReadByte(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, std::string_view label, std::uint16_t address);
bool openCartridge(
    lexxos::os::AppContext& context,
    std::string_view programName,
    std::string_view path,
    lexxos::gameboy::Cartridge& cartridge);

std::string_view gbTestResultName(GbTestRunSummary const& summary);
int gbTestExitCode(GbTestRunSummary const& summary);
bool runGbTestRom(
    lexxos::os::AppContext& context,
    std::string_view programName,
    std::string_view path,
    std::uint32_t cycleLimit,
    bool traceEnabled,
    bool verbose,
    GbTestRunSummary& summary);
bool joinPath(std::string_view directory, std::string_view filename, std::array<char, lexxos::vfs::maxPathLength>& output);

void writeGamepadState(lexxos::os::AppContext& context, lexxos::devices::GamepadState const& state);
void writeCpuState(lexxos::os::AppContext& context, lexxos::gameboy::Cpu const& cpu);
void writeCpuStep(
    lexxos::os::AppContext& context,
    std::uint32_t index,
    lexxos::gameboy::CpuStepResult const& step,
    lexxos::gameboy::Cpu const& cpu);
void writeRunStopStep(lexxos::os::AppContext& context, char const* prefix, RunBudgetResult const& run);
void writeRunTrace(lexxos::os::AppContext& context, char const* prefix, RunTraceBuffer const& trace);
void writeGbEmuFaultSnapshot(
    lexxos::os::AppContext& context,
    char const* reason,
    std::uint32_t frame,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cartridge const& cartridge,
    lexxos::gameboy::FrameBuffer const& framebuffer,
    lexxos::gameboy::BackgroundMapMode bgMapMode,
    lexxos::gameboy::BackgroundTileDataMode bgTileMode,
    RunTraceBuffer const& trace);
void writeDisplayRenderInfo(lexxos::os::AppContext& context, lexxos::gameboy::DisplayRenderInfo const& info);

inline constexpr std::uint32_t fnv1aOffsetBasis = 2166136261U;
std::uint32_t fnv1aUpdate(std::uint32_t hash, std::uint8_t const* data, std::size_t size);
bool readFileBlockRetry(
    lexxos::vfs::FileHandle& file,
    std::uint32_t offset,
    std::uint8_t* buffer,
    std::size_t size,
    std::uint8_t attempts = 4);
void writeRomProbe(lexxos::os::AppContext& context, std::uint32_t offset, std::uint8_t const* data, std::size_t size);

VramFirstNonZero firstNonZeroRange(lexxos::gameboy::Bus& bus, std::uint16_t start, std::uint16_t endInclusive);
std::uint32_t countNonZeroForTarget(lexxos::gameboy::Bus& bus, VramWarmupTarget target);
std::uint32_t countFrameNonWhitePixels(lexxos::gameboy::FrameBuffer const& framebuffer);
void writeBackgroundUsageStats(
    lexxos::os::AppContext& context,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::FrameBuffer const& framebuffer,
    lexxos::gameboy::BackgroundMapMode bgMapMode,
    lexxos::gameboy::BackgroundTileDataMode bgTileMode,
    char const* prefix);
void writeSpriteUsageStats(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, char const* prefix);
void writeVramRangeStats(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus);
void writeCartridgeCacheStats(
    lexxos::os::AppContext& context,
    lexxos::gameboy::Cartridge const& cartridge,
    char const* prefix);
void writeBusIoSnapshot(lexxos::os::AppContext& context, lexxos::gameboy::Bus& bus, char const* prefix);
void writeGbEmuTraceLine(
    lexxos::os::AppContext& context,
    std::uint32_t frame,
    std::uint32_t frameSteps,
    std::uint32_t frameCycles,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cartridge const& cartridge);
void writeGbEmuFinalSummary(
    lexxos::os::AppContext& context,
    lexxos::gameboy::Bus& bus,
    lexxos::gameboy::Cpu const& cpu,
    lexxos::gameboy::Cartridge const& cartridge);

} // namespace lexxos::apps::gameboy_programs
