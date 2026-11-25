#pragma once

namespace lexxos::platform {

struct LegacyBoardDescriptor;
struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::ce::blink {

[[nodiscard]] lexxos::platform::LegacyBoardDescriptor const& boardDescriptor();
[[nodiscard]] lexxos::platform::PlatformServices const& platformServices();

} // namespace lexxos::ce::blink
