#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/app/console_mirror_sink.h>
#include <lexxos/app/scoped_console_mirror_mode.h>
#include <lexxos/app/scoped_console_mirror_suspension.h>

namespace lexxos::os {

class OperatingSystem;

} // namespace lexxos::os

// Suppresses text mirroring without changing focus-selected Text/Graphics mode.
namespace lexxos::app::console_mirror {

void initialize(lexxos::os::OperatingSystem& operatingSystem);
void write(std::string_view text);
void flush();
void clear();
void setMode(ConsoleMirrorMode mode);
bool update();
void refresh();

[[nodiscard]] ConsoleMirrorSink& sink();
[[nodiscard]] ConsoleMirrorMode mode();
[[nodiscard]] bool isReady();
[[nodiscard]] bool isSuspended();

} // namespace lexxos::app::console_mirror
