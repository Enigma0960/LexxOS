#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/program_supervisor.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::os { class AppContext; }

namespace lexxos::launch {

inline constexpr std::string_view shortcutSuffix = ".lnk";
inline constexpr std::string_view autostartConfigPath = "/mnt/sd/.lexx/autostart.conf";
inline constexpr std::size_t maxShortcutArgs = 10;
inline constexpr std::size_t maxShortcutArgBytes = 96;
inline constexpr std::uint32_t maxDelayMs = 600000;

struct Shortcut {
    std::array<char, lexxos::vfs::maxPathLength> target{};
    std::array<std::array<char, maxShortcutArgBytes>, maxShortcutArgs> args{};
    std::size_t argCount{0};
    bool background{false};
    std::uint32_t delayMs{0};
};

bool hasShortcutSuffix(std::string_view path);
bool loadShortcut(lexxos::vfs::Vfs& vfs, std::string_view path, Shortcut& shortcut);
lexxos::vfs::WriteStatus saveShortcut(lexxos::vfs::Vfs& vfs,
    std::string_view path, Shortcut const& shortcut);
lexxos::program::ProgramSupervisorResult launchShortcut(lexxos::os::AppContext& context,
    Shortcut const& shortcut, int extraArgc = 0, char const* const* extraArgv = nullptr);

std::uint32_t readAutostartDelay(lexxos::vfs::Vfs& vfs);
bool writeAutostartDelay(lexxos::vfs::Vfs& vfs, std::uint32_t milliseconds);
void requestAutostartRun();
void pollAutostart(lexxos::os::AppContext& context, std::uint32_t nowMs);

} // namespace lexxos::launch
