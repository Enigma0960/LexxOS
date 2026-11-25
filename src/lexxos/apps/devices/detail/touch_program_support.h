#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs_types.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

class ParsedArguments;

} // namespace lexxos::program

namespace lexxos::devices {

struct Rect;

class PointingDevice;
class DisplayDevice;
struct PointingDeviceState;
struct TouchCalibration;
struct TouchContact;

} // namespace lexxos::devices

namespace lexxos::apps::device_programs::detail::touch {

constexpr std::uint32_t defaultCalibrationSamples = 8;
constexpr std::uint32_t defaultCalibrationTimeoutSeconds = 20;
constexpr std::uint32_t defaultDrawSeconds = 60;
constexpr std::uint32_t defaultDrawRadius = 3;
constexpr std::uint32_t touchPollDelayMs = 20;
constexpr std::uint32_t touchDrawDelayMs = 16;
constexpr std::uint32_t releaseTimeoutMs = 10000;
constexpr int targetMargin = 48;
constexpr std::size_t calibrationTargetCount = 5;
constexpr std::size_t drawTrackCount = 16;

struct CalibrationSample {
	std::int32_t targetX{0};
	std::int32_t targetY{0};
	std::int32_t rawX{0};
	std::int32_t rawY{0};
};

struct DrawTrack {
	bool active{false};
	std::int32_t x{0};
	std::int32_t y{0};
};

void writeSigned(lexxos::os::AppContext& context, std::int32_t value);
std::string_view trim(std::string_view value);
bool parseBool(std::string_view text, bool& value);
bool parseInt32(std::string_view text, std::int32_t& value);
bool appendText(std::array<char, 360>& output, std::size_t& position, std::string_view text);
bool appendChar(std::array<char, 360>& output, std::size_t& position, char value);
bool appendUnsignedText(std::array<char, 360>& output, std::size_t& position, std::uint32_t value);
bool appendSignedText(std::array<char, 360>& output, std::size_t& position, std::int32_t value);
bool appendCalibrationValue(
    std::array<char, 360>& output,
    std::size_t& position,
    std::string_view key,
    std::int32_t value);
bool appendCalibrationBool(
    std::array<char, 360>& output,
    std::size_t& position,
    std::string_view key,
    bool value);
bool makeCalibrationConfigText(
    lexxos::devices::TouchCalibration const& calibration,
    std::array<char, 360>& output,
    std::size_t& size);
bool parseCalibrationConfigText(std::string_view text, lexxos::devices::TouchCalibration& calibration);
bool readCalibrationConfig(lexxos::devices::TouchCalibration& calibration);
bool ensureDirectory(std::string_view path);
lexxos::vfs::WriteStatus saveCalibrationConfig(lexxos::devices::TouchCalibration const& calibration);
lexxos::devices::PointingDevice* selectTouch(
    lexxos::os::AppContext& context,
    std::string_view name,
    std::string_view command);
void writeCalibration(lexxos::os::AppContext& context, lexxos::devices::TouchCalibration const& calibration);
std::int32_t clampInt(std::int32_t value, std::int32_t low, std::int32_t high);
std::int32_t mapAxis(
    std::int32_t value,
    std::int32_t rawMin,
    std::int32_t rawMax,
    std::int32_t displayExtent,
    bool invert);
lexxos::devices::TouchContact mapSample(
    CalibrationSample const& sample,
    lexxos::devices::TouchCalibration const& calibration);
std::int32_t roundToInt(double value);
bool fitRawRange(
    std::array<CalibrationSample, calibrationTargetCount> const& samples,
    std::size_t count,
    bool axisX,
    bool swapXY,
    bool invert,
    std::int32_t displayExtent,
    std::int32_t& rawMin,
    std::int32_t& rawMax);
bool computeCalibration(
    std::array<CalibrationSample, calibrationTargetCount> const& samples,
    std::size_t count,
    int width,
    int height,
    lexxos::devices::TouchCalibration& output);
bool fillClipped(
    lexxos::devices::DisplayDevice& display,
    lexxos::devices::Rect rect,
    std::uint16_t color);
void drawCalibrationTarget(lexxos::devices::DisplayDevice& display, int x, int y);
void recordDisplayActivityForTouch(lexxos::os::AppContext& context);
bool waitForRelease(
    lexxos::os::AppContext& context,
    lexxos::devices::PointingDevice& touch,
    std::uint32_t timeoutMs);
bool captureCalibrationSample(
    lexxos::os::AppContext& context,
    lexxos::devices::PointingDevice& touch,
    std::uint32_t samples,
    std::uint32_t timeoutMs,
    CalibrationSample& output);
bool parseActionAndName(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view& action,
    std::string_view& name);
int restoreCalibration(
    lexxos::os::AppContext& context,
    std::string_view name,
    std::string_view command);
int showCalibration(
    lexxos::os::AppContext& context,
    std::string_view name,
    std::string_view command);
void drawDot(lexxos::devices::DisplayDevice& display, std::int32_t x, std::int32_t y, std::int32_t radius, std::uint16_t color);
void drawLine(
    lexxos::devices::DisplayDevice& display,
    std::int32_t x0,
    std::int32_t y0,
    std::int32_t x1,
    std::int32_t y1,
    std::int32_t radius,
    std::uint16_t color);

} // namespace lexxos::apps::device_programs::detail::touch
