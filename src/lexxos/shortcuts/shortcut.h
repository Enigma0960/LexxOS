#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::shortcuts {

inline constexpr std::size_t maxShortcutArguments = 8;
inline constexpr std::size_t maxArgumentBytes = 96;
inline constexpr std::uint32_t maxDelayMs = 86400000U;
inline constexpr std::string_view configPath = "/mnt/sd/.lexx/autostart.conf";

struct Shortcut {
	std::array<char, lexxos::vfs::maxPathLength> target{};
	std::array<std::array<char, maxArgumentBytes>, maxShortcutArguments> args{};
	std::size_t argc{0};
	std::uint32_t delayMs{0};
	bool background{false};
};

struct Config {
	std::uint32_t delayMs{1000};
	bool enabled{true};
};

enum class Status : std::uint8_t {
	Ok,
	NotFound,
	Invalid,
	TooLarge,
	IoError,
	LaunchError
};

bool isShortcut(std::string_view path);
bool parseDelay(std::string_view text, std::uint32_t& result);
Status load(vfs::Vfs& vfs, std::string_view path, Shortcut& result);
Status load(std::string_view path, Shortcut& result);
Status save(std::string_view path, Shortcut const& data);
Status launch(lexxos::os::AppContext& context, Shortcut const& data,
    int extraArgc = 0, char const* const* extraArgv = nullptr);
Config loadConfig();
bool saveConfig(Config const& config);
void pollBoot(lexxos::os::AppContext& context);
std::string_view statusText(Status value);

} // namespace lexxos::shortcuts
