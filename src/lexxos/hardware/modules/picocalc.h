#pragma once

namespace lexxos::hardware::modules::picocalc {

// Visible area and orientation used by the existing ILI9488 runtime adapter.
// No controller initialization or firmware-revision inference belongs here.
inline constexpr int displayWidth = 320;
inline constexpr int displayHeight = 320;
inline constexpr bool displayInvertColors = true;

} // namespace lexxos::hardware::modules::picocalc
