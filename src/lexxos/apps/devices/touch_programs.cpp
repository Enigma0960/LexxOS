#include <commands/command_selection.h>

#include <lexxos/apps/devices/touch_programs.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::device_programs {
namespace {

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

	constexpr lexxos::program::ProgramArgumentSpec optionalTouchName[] = {
	    {
         .name = "name",
         .valueName = "name",
         .help = "touch-capable pointing device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	constexpr lexxos::program::ProgramArgumentSpec touchCalibrateArguments[] = {
	    {
         .name = "samples",
         .shortName = 'n',
         .valueName = "count",
         .help = "samples averaged at each calibration target",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "8",
	     },
	    {
         .name = "timeout",
         .shortName = 't',
         .valueName = "seconds",
         .help = "timeout for each target",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "20",
	     },
	    {
         .name = "display",
         .shortName = 'd',
         .valueName = "name",
         .help = "display device name",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "no-save",
         .help = "apply calibration without saving it to SD",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "action",
         .valueName = "calibrate|restore|show",
         .help = "calibration action",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "touch-capable pointing device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	constexpr lexxos::program::ProgramArgumentSpec touchDrawArguments[] = {
	    {
         .name = "seconds",
         .shortName = 's',
         .valueName = "seconds",
         .help = "test duration",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "60",
	     },
	    {
         .name = "radius",
         .shortName = 'r',
         .valueName = "pixels",
         .help = "brush radius",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "3",
	     },
	    {
         .name = "display",
         .shortName = 'd',
         .valueName = "name",
         .help = "display device name",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "touch-capable pointing device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeSigned(lexxos::os::AppContext& context, std::int32_t value) {
		if (value < 0) {
			context.write("-");
			writeUnsigned(context, static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value))));
			return;
		}
		writeUnsigned(context, static_cast<std::uint32_t>(value));
	}

	std::string_view trim(std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
			value.remove_suffix(1);
		}
		return value;
	}

	bool parseBool(std::string_view text, bool& value) {
		if (text == "1" || text == "yes" || text == "true" || text == "on") {
			value = true;
			return true;
		}
		if (text == "0" || text == "no" || text == "false" || text == "off") {
			value = false;
			return true;
		}
		return false;
	}

	bool parseInt32(std::string_view text, std::int32_t& value) {
		bool negative = false;
		if (!text.empty() && text.front() == '-') {
			negative = true;
			text.remove_prefix(1);
		}
		std::uint32_t parsed = 0;
		if (!parseUnsigned32(text, parsed) || (!negative && parsed > 0x7FFFFFFFU) || (negative && parsed > 0x80000000U)) {
			return false;
		}
		value = negative ? static_cast<std::int32_t>(-(static_cast<std::int64_t>(parsed))) : static_cast<std::int32_t>(parsed);
		return true;
	}

	bool appendText(std::array<char, 360>& output, std::size_t& position, std::string_view text) {
		if (text.size() > output.size() - position) {
			return false;
		}
		for (auto const character : text) {
			output[position++] = character;
		}
		return true;
	}

	bool appendChar(std::array<char, 360>& output, std::size_t& position, char value) {
		if (position >= output.size()) {
			return false;
		}
		output[position++] = value;
		return true;
	}

	bool appendUnsignedText(std::array<char, 360>& output, std::size_t& position, std::uint32_t value) {
		char digits[10]{};
		std::size_t digitCount = 0;
		do {
			digits[digitCount++] = static_cast<char>('0' + value % 10U);
			value /= 10U;
		} while (value != 0);
		while (digitCount != 0) {
			if (!appendChar(output, position, digits[--digitCount])) {
				return false;
			}
		}
		return true;
	}

	bool appendSignedText(std::array<char, 360>& output, std::size_t& position, std::int32_t value) {
		if (value < 0 && !appendChar(output, position, '-')) {
			return false;
		}
		auto const magnitude = value < 0
		    ? static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value)))
		    : static_cast<std::uint32_t>(value);
		return appendUnsignedText(output, position, magnitude);
	}

	bool appendCalibrationValue(
	    std::array<char, 360>& output,
	    std::size_t& position,
	    std::string_view key,
	    std::int32_t value) {
		return appendText(output, position, key)
		    && appendText(output, position, "=")
		    && appendSignedText(output, position, value)
		    && appendChar(output, position, '\n');
	}

	bool appendCalibrationBool(
	    std::array<char, 360>& output,
	    std::size_t& position,
	    std::string_view key,
	    bool value) {
		return appendText(output, position, key)
		    && appendText(output, position, value ? "=1\n" : "=0\n");
	}

	bool makeCalibrationConfigText(
	    lexxos::devices::TouchCalibration const& calibration,
	    std::array<char, 360>& output,
	    std::size_t& size) {
		size = 0;
		return appendText(output, size, "# LexxOS touch calibration\n")
		    && appendCalibrationBool(output, size, "valid", calibration.valid)
		    && appendCalibrationBool(output, size, "swap_xy", calibration.swapXY)
		    && appendCalibrationBool(output, size, "invert_x", calibration.invertX)
		    && appendCalibrationBool(output, size, "invert_y", calibration.invertY)
		    && appendCalibrationValue(output, size, "raw_min_x", calibration.rawMinX)
		    && appendCalibrationValue(output, size, "raw_max_x", calibration.rawMaxX)
		    && appendCalibrationValue(output, size, "raw_min_y", calibration.rawMinY)
		    && appendCalibrationValue(output, size, "raw_max_y", calibration.rawMaxY)
		    && appendCalibrationValue(output, size, "display_width", calibration.displayWidth)
		    && appendCalibrationValue(output, size, "display_height", calibration.displayHeight);
	}

	bool parseCalibrationConfigText(std::string_view text, lexxos::devices::TouchCalibration& calibration) {
		calibration = {};
		bool hasValid = false;
		while (!text.empty()) {
			auto const newline = text.find('\n');
			auto line = newline == std::string_view::npos ? text : text.substr(0, newline);
			text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);

			line = trim(line);
			if (line.empty() || line.front() == '#') {
				continue;
			}
			auto const separator = line.find('=');
			if (separator == std::string_view::npos) {
				return false;
			}
			auto const key = trim(line.substr(0, separator));
			auto const value = trim(line.substr(separator + 1));

			if (key == "valid") {
				if (!parseBool(value, calibration.valid)) {
					return false;
				}
				hasValid = true;
			} else if (key == "swap_xy") {
				if (!parseBool(value, calibration.swapXY)) {
					return false;
				}
			} else if (key == "invert_x") {
				if (!parseBool(value, calibration.invertX)) {
					return false;
				}
			} else if (key == "invert_y") {
				if (!parseBool(value, calibration.invertY)) {
					return false;
				}
			} else if (key == "raw_min_x") {
				if (!parseInt32(value, calibration.rawMinX)) {
					return false;
				}
			} else if (key == "raw_max_x") {
				if (!parseInt32(value, calibration.rawMaxX)) {
					return false;
				}
			} else if (key == "raw_min_y") {
				if (!parseInt32(value, calibration.rawMinY)) {
					return false;
				}
			} else if (key == "raw_max_y") {
				if (!parseInt32(value, calibration.rawMaxY)) {
					return false;
				}
			} else if (key == "display_width") {
				if (!parseInt32(value, calibration.displayWidth)) {
					return false;
				}
			} else if (key == "display_height") {
				if (!parseInt32(value, calibration.displayHeight)) {
					return false;
				}
			} else {
				return false;
			}
		}
		return hasValid && calibration.valid && calibration.rawMinX != calibration.rawMaxX && calibration.rawMinY != calibration.rawMaxY && calibration.displayWidth > 1 && calibration.displayHeight > 1;
	}

	bool readCalibrationConfig(lexxos::devices::TouchCalibration& calibration) {
		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(touchCalibrationConfigPath, lexxos::vfs::OpenMode::Read, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			return false;
		}
		std::array<char, 360> buffer{};
		std::size_t bytesRead = 0;
		auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
		return readStatus == lexxos::vfs::ReadStatus::Ok
		    && parseCalibrationConfigText(std::string_view{buffer.data(), bytesRead}, calibration);
	}

	bool ensureDirectory(std::string_view path) {
		lexxos::vfs::Stat node{};
		if (lexxos::vfs::stat(path, node)) {
			return node.type == lexxos::vfs::NodeType::Directory;
		}
		auto const status = lexxos::vfs::createDirectory(path);
		return status == lexxos::vfs::WriteStatus::Ok || status == lexxos::vfs::WriteStatus::IsDirectory;
	}

	lexxos::vfs::WriteStatus saveCalibrationConfig(lexxos::devices::TouchCalibration const& calibration) {
		if (!ensureDirectory(touchCalibrationConfigDirectory)) {
			return lexxos::vfs::WriteStatus::NotFound;
		}
		std::array<char, 360> buffer{};
		std::size_t size = 0;
		if (!makeCalibrationConfigText(calibration, buffer, size)) {
			return lexxos::vfs::WriteStatus::TooLarge;
		}
		auto const writeStatus = lexxos::vfs::writeText(
		    touchCalibrationConfigPath,
		    std::string_view{buffer.data(), size},
		    lexxos::vfs::WriteMode::CreateTruncate);
		if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
			return writeStatus;
		}
		return lexxos::vfs::sync() == lexxos::vfs::SyncStatus::Ok
		    ? lexxos::vfs::WriteStatus::Ok
		    : lexxos::vfs::WriteStatus::NotWritable;
	}

	lexxos::devices::PointingDevice* selectTouch(
	    lexxos::os::AppContext& context,
	    std::string_view name,
	    std::string_view command) {
		auto& devices = context.devices();
		lexxos::devices::PointingDevice* touch = nullptr;
		if (!name.empty()) {
			touch = devices.pointingDevice(name);
		} else {
			for (std::size_t index = 0; index < devices.pointingDeviceCount(); ++index) {
				auto& descriptor = devices.pointingDeviceAt(index);
				if (descriptor.device != nullptr && descriptor.device->supportsTouch()) {
					touch = descriptor.device;
					break;
				}
			}
		}

		if (touch == nullptr) {
			context.writeError(command);
			context.writeErrorLine(": touch device is not registered");
			return nullptr;
		}
		if (!touch->supportsTouch()) {
			context.writeError(command);
			context.writeErrorLine(": pointing device does not support touch");
			return nullptr;
		}
		if (!touch->isReady()) {
			context.writeError(command);
			context.writeErrorLine(": touch device is not ready");
			return nullptr;
		}
		return touch;
	}

	void writeCalibration(lexxos::os::AppContext& context, lexxos::devices::TouchCalibration const& calibration) {
		context.write("calibration: valid=");
		context.write(calibration.valid ? "yes" : "no");
		context.write(" swap-xy=");
		context.write(calibration.swapXY ? "yes" : "no");
		context.write(" invert-x=");
		context.write(calibration.invertX ? "yes" : "no");
		context.write(" invert-y=");
		context.write(calibration.invertY ? "yes" : "no");
		context.write(" raw-x=");
		writeSigned(context, calibration.rawMinX);
		context.write("..");
		writeSigned(context, calibration.rawMaxX);
		context.write(" raw-y=");
		writeSigned(context, calibration.rawMinY);
		context.write("..");
		writeSigned(context, calibration.rawMaxY);
		context.write(" display=");
		writeSigned(context, calibration.displayWidth);
		context.write("x");
		writeSigned(context, calibration.displayHeight);
		context.writeLine("");
	}

	std::int32_t clampInt(std::int32_t value, std::int32_t low, std::int32_t high) {
		if (value < low) {
			return low;
		}
		if (value > high) {
			return high;
		}
		return value;
	}

	std::int32_t mapAxis(
	    std::int32_t value,
	    std::int32_t rawMin,
	    std::int32_t rawMax,
	    std::int32_t displayExtent,
	    bool invert) {
		if (displayExtent <= 1 || rawMin == rawMax) {
			return 0;
		}
		if (rawMin > rawMax) {
			auto const temporary = rawMin;
			rawMin = rawMax;
			rawMax = temporary;
		}
		auto const clamped = clampInt(value, rawMin, rawMax);
		auto const range = static_cast<std::int64_t>(rawMax - rawMin);
		auto mapped = static_cast<std::int32_t>(
		    (static_cast<std::int64_t>(clamped - rawMin) * (displayExtent - 1) + range / 2) / range);
		if (invert) {
			mapped = displayExtent - 1 - mapped;
		}
		return clampInt(mapped, 0, displayExtent - 1);
	}

	lexxos::devices::TouchContact mapSample(
	    CalibrationSample const& sample,
	    lexxos::devices::TouchCalibration const& calibration) {
		auto const axisX = calibration.swapXY ? sample.rawY : sample.rawX;
		auto const axisY = calibration.swapXY ? sample.rawX : sample.rawY;
		return {
		    .active = true,
		    .x = mapAxis(axisX, calibration.rawMinX, calibration.rawMaxX, calibration.displayWidth, calibration.invertX),
		    .y = mapAxis(axisY, calibration.rawMinY, calibration.rawMaxY, calibration.displayHeight, calibration.invertY),
		    .rawX = sample.rawX,
		    .rawY = sample.rawY,
		};
	}

	std::int32_t roundToInt(double value) {
		return static_cast<std::int32_t>(value >= 0.0 ? value + 0.5 : value - 0.5);
	}

	bool fitRawRange(
	    std::array<CalibrationSample, calibrationTargetCount> const& samples,
	    std::size_t count,
	    bool axisX,
	    bool swapXY,
	    bool invert,
	    std::int32_t displayExtent,
	    std::int32_t& rawMin,
	    std::int32_t& rawMax) {
		double sumTarget = 0.0;
		double sumRaw = 0.0;
		double sumTargetTarget = 0.0;
		double sumTargetRaw = 0.0;
		for (std::size_t index = 0; index < count; ++index) {
			auto const& sample = samples[index];
			auto const raw = axisX
			    ? (swapXY ? sample.rawY : sample.rawX)
			    : (swapXY ? sample.rawX : sample.rawY);
			auto const target = axisX ? sample.targetX : sample.targetY;
			auto const normalizedTarget = invert ? displayExtent - 1 - target : target;
			sumTarget += static_cast<double>(normalizedTarget);
			sumRaw += static_cast<double>(raw);
			sumTargetTarget += static_cast<double>(normalizedTarget) * static_cast<double>(normalizedTarget);
			sumTargetRaw += static_cast<double>(normalizedTarget) * static_cast<double>(raw);
		}

		auto const n = static_cast<double>(count);
		auto const denominator = n * sumTargetTarget - sumTarget * sumTarget;
		if (denominator > -0.001 && denominator < 0.001) {
			return false;
		}
		auto const slope = (n * sumTargetRaw - sumTarget * sumRaw) / denominator;
		auto const intercept = (sumRaw - slope * sumTarget) / n;
		if (slope <= 0.001) {
			return false;
		}
		rawMin = roundToInt(intercept);
		rawMax = roundToInt(intercept + slope * static_cast<double>(displayExtent - 1));
		return rawMax > rawMin;
	}

	bool computeCalibration(
	    std::array<CalibrationSample, calibrationTargetCount> const& samples,
	    std::size_t count,
	    int width,
	    int height,
	    lexxos::devices::TouchCalibration& output) {
		bool hasBest = false;
		std::int64_t bestScore = 0;
		lexxos::devices::TouchCalibration best{};
		for (int swapIndex = 0; swapIndex < 2; ++swapIndex) {
			auto const swapXY = swapIndex != 0;
			for (int invertXIndex = 0; invertXIndex < 2; ++invertXIndex) {
				auto const invertX = invertXIndex != 0;
				for (int invertYIndex = 0; invertYIndex < 2; ++invertYIndex) {
					auto const invertY = invertYIndex != 0;
					lexxos::devices::TouchCalibration candidate{
					    .valid = true,
					    .swapXY = swapXY,
					    .invertX = invertX,
					    .invertY = invertY,
					    .displayWidth = width,
					    .displayHeight = height,
					};
					if (!fitRawRange(samples, count, true, swapXY, invertX, width, candidate.rawMinX, candidate.rawMaxX)
					    || !fitRawRange(samples, count, false, swapXY, invertY, height, candidate.rawMinY, candidate.rawMaxY)) {
						continue;
					}
					std::int64_t score = 0;
					for (std::size_t index = 0; index < count; ++index) {
						auto const mapped = mapSample(samples[index], candidate);
						auto const dx = static_cast<std::int64_t>(mapped.x - samples[index].targetX);
						auto const dy = static_cast<std::int64_t>(mapped.y - samples[index].targetY);
						score += dx * dx + dy * dy;
					}
					if (!hasBest || score < bestScore) {
						hasBest = true;
						bestScore = score;
						best = candidate;
					}
				}
			}
		}
		if (!hasBest) {
			return false;
		}
		output = best;
		return true;
	}

	bool fillClipped(
	    lexxos::devices::DisplayDevice& display,
	    lexxos::devices::Rect rect,
	    std::uint16_t color) {
		if (rect.width <= 0 || rect.height <= 0) {
			return false;
		}
		if (rect.x < 0) {
			rect.width += rect.x;
			rect.x = 0;
		}
		if (rect.y < 0) {
			rect.height += rect.y;
			rect.y = 0;
		}
		if (rect.x + rect.width > display.width()) {
			rect.width = display.width() - rect.x;
		}
		if (rect.y + rect.height > display.height()) {
			rect.height = display.height() - rect.y;
		}
		if (rect.width <= 0 || rect.height <= 0) {
			return false;
		}
		return display.fillRgb565(rect, color) || detail::drawFilledRect(display, rect, color);
	}

	void drawCalibrationTarget(lexxos::devices::DisplayDevice& display, int x, int y) {
		auto const background = detail::rgb565(8, 10, 14);
		auto const line = detail::rgb565(238, 244, 255);
		auto const center = detail::rgb565(250, 80, 80);
		(void) display.clear(background);
		(void) fillClipped(display, {.x = x - 26, .y = y - 2, .width = 53, .height = 5}, line);
		(void) fillClipped(display, {.x = x - 2, .y = y - 26, .width = 5, .height = 53}, line);
		(void) fillClipped(display, {.x = x - 7, .y = y - 7, .width = 15, .height = 15}, center);
	}

	void recordDisplayActivityForTouch(lexxos::os::AppContext& context) {
		if (context.capabilities().has(lexxos::runtime::Capability::DisplayRgb565)) {
			lexxos::app::picocalc_display_power::recordActivity(context.platform());
		}
	}

	bool waitForRelease(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PointingDevice& touch,
	    std::uint32_t timeoutMs) {
		auto const started = uptimeMs(context);
		while (uptimeMs(context) - started < timeoutMs) {
			if (context.pollCancellationRequest()) {
				return false;
			}
			if (touch.touchState().contactCount == 0) {
				return true;
			}
			recordDisplayActivityForTouch(context);
			lexxos::kernel::sleepMs(touchPollDelayMs);
		}
		return false;
	}

	bool captureCalibrationSample(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PointingDevice& touch,
	    std::uint32_t samples,
	    std::uint32_t timeoutMs,
	    CalibrationSample& output) {
		auto const started = uptimeMs(context);
		std::uint32_t captured = 0;
		std::int64_t sumX = 0;
		std::int64_t sumY = 0;
		while (uptimeMs(context) - started < timeoutMs) {
			if (context.pollCancellationRequest()) {
				return false;
			}
			auto const state = touch.touchState();
			if (state.contactCount == 1 && state.contacts[0].active) {
				recordDisplayActivityForTouch(context);
				sumX += state.contacts[0].rawX;
				sumY += state.contacts[0].rawY;
				++captured;
				if (captured >= samples) {
					output.rawX = static_cast<std::int32_t>((sumX + captured / 2U) / captured);
					output.rawY = static_cast<std::int32_t>((sumY + captured / 2U) / captured);
					return waitForRelease(context, touch, releaseTimeoutMs);
				}
			} else {
				captured = 0;
				sumX = 0;
				sumY = 0;
			}
			lexxos::kernel::sleepMs(touchPollDelayMs);
		}
		return false;
	}

	bool parseActionAndName(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view& action,
	    std::string_view& name) {
		auto const first = arguments.positional("action");
		auto const second = arguments.positional("name");
		action = "calibrate";
		name = second;
		if (first.empty()) {
			return true;
		}
		if (first == "calibrate" || first == "run" || first == "restore" || first == "load" || first == "show") {
			action = first == "load" ? std::string_view{"restore"} : first;
			return true;
		}
		if (!second.empty()) {
			return false;
		}
		name = first;
		return true;
	}

	int restoreCalibration(
	    lexxos::os::AppContext& context,
	    std::string_view name,
	    std::string_view command) {
		auto* touch = selectTouch(context, name, command);
		if (touch == nullptr) {
			return 1;
		}
		lexxos::devices::TouchCalibration calibration{};
		if (!readCalibrationConfig(calibration)) {
			context.writeError(command);
			context.writeError(": cannot read ");
			context.writeErrorLine(touchCalibrationConfigPath);
			return 1;
		}
		auto lock = context.devices().lockPointingDevice(touch->name(), lexxos::devices::DeviceResourcePriority::System, command);
		if (!lock) {
			context.writeError(command);
			context.writeErrorLine(": touch device is busy");
			return 1;
		}
		if (!touch->applyTouchCalibration(calibration)) {
			context.writeError(command);
			context.writeErrorLine(": invalid saved calibration");
			return 1;
		}
		context.write(command);
		context.write(": restored path=");
		context.writeLine(touchCalibrationConfigPath);
		writeCalibration(context, calibration);
		return 0;
	}

	int showCalibration(
	    lexxos::os::AppContext& context,
	    std::string_view name,
	    std::string_view command) {
		auto* touch = selectTouch(context, name, command);
		if (touch == nullptr) {
			return 1;
		}
		context.write("device: ");
		context.writeLine(touch->name());
		writeCalibration(context, touch->touchCalibration());
		context.write("path: ");
		context.writeLine(touchCalibrationConfigPath);
		return 0;
	}

	void drawDot(lexxos::devices::DisplayDevice& display, std::int32_t x, std::int32_t y, std::int32_t radius, std::uint16_t color) {
		(void) fillClipped(
		    display,
		    {
		        .x = static_cast<int>(x - radius),
		        .y = static_cast<int>(y - radius),
		        .width = static_cast<int>(radius * 2 + 1),
		        .height = static_cast<int>(radius * 2 + 1),
		    },
		    color);
	}

	void drawLine(
	    lexxos::devices::DisplayDevice& display,
	    std::int32_t x0,
	    std::int32_t y0,
	    std::int32_t x1,
	    std::int32_t y1,
	    std::int32_t radius,
	    std::uint16_t color) {
		auto dx = x1 > x0 ? x1 - x0 : x0 - x1;
		auto sx = x0 < x1 ? 1 : -1;
		auto dy = y1 > y0 ? y0 - y1 : y1 - y0;
		auto sy = y0 < y1 ? 1 : -1;
		auto error = dx + dy;
		while (true) {
			drawDot(display, x0, y0, radius, color);
			if (x0 == x1 && y0 == y1) {
				break;
			}
			auto const e2 = 2 * error;
			if (e2 >= dy) {
				error += dy;
				x0 += sx;
			}
			if (e2 <= dx) {
				error += dx;
				y0 += sy;
			}
		}
	}

} // namespace

#if LEXXOS_COMMAND_TOUCH_INFO
lexxos::program::ProgramDescriptor TouchInfoProgram::descriptor() {
	return {
	    .name = "touch-info",
	    .help = "show touch panel state",
	    .description = "Prints touch-capable pointing device contacts, raw coordinates and calibration.",
	    .usage = "touch-info [name]",
	    .arguments = argumentList(optionalTouchName),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}
#endif

#if LEXXOS_COMMAND_TOUCH_INFO
int TouchInfoProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto* touch = selectTouch(context, arguments.positional("name"), "touch-info");
	if (touch == nullptr) {
		return 1;
	}
	auto state = touch->touchState();
	context.write("device: ");
	context.write(touch->name());
	context.write(" connected=");
	context.write(state.connected ? "yes" : "no");
	context.write(" contacts=");
	writeUnsigned(context, state.contactCount);
	context.write("/");
	writeUnsigned(context, state.maxContacts);
	context.write(" reports=");
	writeUnsigned(context, state.reportCount);
	context.write(" last=");
	writeUnsigned(context, state.lastReportTimestampMs);
	context.writeLine("ms");
	for (std::uint8_t index = 0; index < state.contactCount && index < lexxos::devices::maxTouchContacts; ++index) {
		auto const& contact = state.contacts[index];
		context.write("contact[");
		writeUnsigned(context, index);
		context.write("]: id=");
		writeUnsigned(context, contact.id);
		context.write(" x=");
		writeSigned(context, contact.x);
		context.write(" y=");
		writeSigned(context, contact.y);
		context.write(" raw=");
		writeSigned(context, contact.rawX);
		context.write(",");
		writeSigned(context, contact.rawY);
		context.write(" size=");
		writeUnsigned(context, contact.size);
		context.writeLine("");
	}
	writeCalibration(context, state.calibration);
	context.write("path: ");
	context.writeLine(touchCalibrationConfigPath);
	return 0;
}
#endif

#if LEXXOS_COMMAND_TOUCH_CALIBRATE
lexxos::program::ProgramDescriptor TouchCalibrateProgram::descriptor() {
	return {
	    .name = "touch-calibrate",
	    .help = "calibrate touch panel",
	    .description = "Captures screen targets, applies touch orientation/range calibration and saves it to SD.",
	    .usage = "touch-calibrate [restore|show|calibrate] [name] [-d display] [-n samples] [-t seconds] [--no-save]",
	    .arguments = argumentList(touchCalibrateArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}
#endif

#if LEXXOS_COMMAND_TOUCH_CALIBRATE
int TouchCalibrateProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::string_view action{};
	std::string_view name{};
	if (!parseActionAndName(arguments, action, name)) {
		context.writeErrorLine("touch-calibrate: invalid arguments");
		return 2;
	}
	if (action == "restore") {
		return restoreCalibration(context, name, "touch-calibrate");
	}
	if (action == "show") {
		return showCalibration(context, name, "touch-calibrate");
	}

	std::uint32_t samples = defaultCalibrationSamples;
	std::uint32_t timeoutSeconds = defaultCalibrationTimeoutSeconds;
	if (!parseUnsigned32(arguments.valueOr("samples", "8"), samples) || samples == 0 || samples > 64) {
		context.writeErrorLine("touch-calibrate: invalid samples value");
		return 2;
	}
	if (!parseUnsigned32(arguments.valueOr("timeout", "20"), timeoutSeconds) || timeoutSeconds == 0 || timeoutSeconds > 300) {
		context.writeErrorLine("touch-calibrate: invalid timeout value");
		return 2;
	}

	auto* touch = selectTouch(context, name, "touch-calibrate");
	if (touch == nullptr) {
		return 1;
	}
	auto* display = detail::selectedDisplay(context, arguments.valueOr("display", ""));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "touch-calibrate: display device is not registered"
		        : "touch-calibrate: display device is not ready");
		return 1;
	}

	auto displayLock = detail::lockDisplay(context, *display, "touch-calibrate");
	if (!displayLock) {
		context.writeErrorLine("touch-calibrate: display device is busy");
		return 1;
	}
	auto touchLock = context.devices().lockPointingDevice(touch->name(), lexxos::devices::DeviceResourcePriority::Interactive, "touch-calibrate");
	if (!touchLock) {
		context.writeErrorLine("touch-calibrate: touch device is busy");
		return 1;
	}

	std::array<CalibrationSample, calibrationTargetCount> captured{};
	captured[0].targetX = targetMargin;
	captured[0].targetY = targetMargin;
	captured[1].targetX = display->width() - targetMargin - 1;
	captured[1].targetY = targetMargin;
	captured[2].targetX = display->width() - targetMargin - 1;
	captured[2].targetY = display->height() - targetMargin - 1;
	captured[3].targetX = targetMargin;
	captured[3].targetY = display->height() - targetMargin - 1;
	captured[4].targetX = display->width() / 2;
	captured[4].targetY = display->height() / 2;

	bool ok = true;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		for (std::size_t index = 0; index < captured.size(); ++index) {
			drawCalibrationTarget(*display, captured[index].targetX, captured[index].targetY);
			if (!captureCalibrationSample(context, *touch, samples, timeoutSeconds * 1000U, captured[index])) {
				ok = false;
				break;
			}
		}
		if (ok) {
			(void) display->clear(detail::rgb565(10, 40, 26));
		}
	}
	if (!ok) {
		context.writeErrorLine(context.cancellationRequested()
		        ? "touch-calibrate: canceled"
		        : "touch-calibrate: target timeout or touch was not released");
		return context.cancellationRequested() ? 130 : 1;
	}

	lexxos::devices::TouchCalibration calibration{};
	if (!computeCalibration(captured, captured.size(), display->width(), display->height(), calibration)) {
		context.writeErrorLine("touch-calibrate: failed to compute calibration");
		return 1;
	}
	if (!touch->applyTouchCalibration(calibration)) {
		context.writeErrorLine("touch-calibrate: failed to apply calibration");
		return 1;
	}

	context.writeLine("touch-calibrate: applied");
	writeCalibration(context, calibration);
	if (!arguments.has("no-save")) {
		auto const status = saveCalibrationConfig(calibration);
		if (status != lexxos::vfs::WriteStatus::Ok) {
			context.writeError("touch-calibrate: save failed: ");
			context.writeErrorLine(lexxos::vfs::toString(status));
			return 1;
		}
		context.write("touch-calibrate: saved path=");
		context.writeLine(touchCalibrationConfigPath);
	}
	return 0;
}
#endif

#if LEXXOS_COMMAND_TOUCH_DRAW
lexxos::program::ProgramDescriptor TouchDrawProgram::descriptor() {
	return {
	    .name = "touch-draw",
	    .help = "draw with one or more fingers",
	    .description = "Runs a full-screen touch drawing test with one colored stroke per active contact id.",
	    .usage = "touch-draw [name] [-d display] [-s seconds] [-r radius]",
	    .arguments = argumentList(touchDrawArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 5120,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = false,
	                .interfaceKind = lexxos::program::ProgramInterfaceKind::Graphical,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}
#endif

#if LEXXOS_COMMAND_TOUCH_DRAW
int TouchDrawProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::uint32_t seconds = defaultDrawSeconds;
	std::uint32_t radius = defaultDrawRadius;
	if (!parseUnsigned32(arguments.valueOr("seconds", "60"), seconds) || seconds == 0 || seconds > 3600) {
		context.writeErrorLine("touch-draw: invalid seconds value");
		return 2;
	}
	if (!parseUnsigned32(arguments.valueOr("radius", "3"), radius) || radius == 0 || radius > 24) {
		context.writeErrorLine("touch-draw: invalid radius value");
		return 2;
	}

	auto* touch = selectTouch(context, arguments.positional("name"), "touch-draw");
	if (touch == nullptr) {
		return 1;
	}
	auto* display = detail::selectedDisplay(context, arguments.valueOr("display", ""));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "touch-draw: display device is not registered"
		        : "touch-draw: display device is not ready");
		return 1;
	}

	auto displayLock = detail::lockDisplay(context, *display, "touch-draw");
	if (!displayLock) {
		context.writeErrorLine("touch-draw: display device is busy");
		return 1;
	}
	auto touchLock = context.devices().lockPointingDevice(touch->name(), lexxos::devices::DeviceResourcePriority::Interactive, "touch-draw");
	if (!touchLock) {
		context.writeErrorLine("touch-draw: touch device is busy");
		return 1;
	}

	constexpr std::uint16_t palette[] = {
	    0xF800,
	    0x07E0,
	    0x001F,
	    0xFFE0,
	    0xF81F,
	    0x07FF,
	    0xFD20,
	    0xAFE5,
	    0xFC9F,
	    0x867D,
	    0xA145,
	    0x5FE0,
	    0xFEA0,
	    0xB7E0,
	    0xF81F,
	    0xFFFF,
	};
	std::array<DrawTrack, drawTrackCount> tracks{};
	auto const started = uptimeMs(context);
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		(void) display->clear(0x0000);
		while (uptimeMs(context) - started < seconds * 1000U) {
			if (context.pollCancellationRequest()) {
				context.writeLine("touch-draw: canceled");
				return 130;
			}
			std::array<bool, drawTrackCount> activeNow{};
			auto const state = touch->touchState();
			if (state.contactCount != 0) {
				recordDisplayActivityForTouch(context);
			}
			for (std::uint8_t index = 0; index < state.contactCount && index < lexxos::devices::maxTouchContacts; ++index) {
				auto const& contact = state.contacts[index];
				if (!contact.active) {
					continue;
				}
				auto const trackIndex = contact.id % drawTrackCount;
				activeNow[trackIndex] = true;
				auto& track = tracks[trackIndex];
				auto const color = palette[trackIndex % (sizeof(palette) / sizeof(palette[0]))];
				if (track.active) {
					drawLine(*display, track.x, track.y, contact.x, contact.y, static_cast<std::int32_t>(radius), color);
				} else {
					drawDot(*display, contact.x, contact.y, static_cast<std::int32_t>(radius), color);
				}
				track.active = true;
				track.x = contact.x;
				track.y = contact.y;
			}
			for (std::size_t index = 0; index < tracks.size(); ++index) {
				if (!activeNow[index]) {
					tracks[index].active = false;
				}
			}
			lexxos::kernel::sleepMs(touchDrawDelayMs);
		}
	}
	context.writeLine("touch-draw: done");
	return 0;
}
#endif

} // namespace lexxos::apps::device_programs
