#include <lexxos/app/runtime_tasks.h>
#include <lexxos/app/software_rtc_task.h>
#include <lexxos/app/wifi_autoconnect.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/console_output.h>
#include <lexxos/app/detail/lcd_console_target.h>
#include <lexxos/app/program_input.h>
#include <lexxos/app/system_event_service.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/runtime/runtime_profile.h>
#include <lexxos/shell/shell.h>

#if FEATURE_ENABLED(PICO_CALC) || FEATURE_ENABLED(CARDPUTER) || (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
	#define LEXXOS_LOCAL_CONSOLE_INPUT 1
#else
	#define LEXXOS_LOCAL_CONSOLE_INPUT 0
#endif

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	#include <lexxos/app/picocalc_lcd_console.h>
#endif

#if FEATURE_ENABLED(DISPLAY)
	#include <lexxos/app/picocalc_display_power.h>
#endif

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	#include <lexxos/app/debug_status_display.h>
#endif

#if LEXXOS_LOCAL_CONSOLE_INPUT
	#include <lexxos/devices/input_device.h>
#endif

#if FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/pointing_device.h>
#endif

#if FEATURE_ENABLED(PICO_CALC) && FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED && defined(LEXXOS_PLATFORM_ESPIDF)
	#include <lexxos/apps/network/detail/wifi_program_support.h>
	#define LEXXOS_LCD_CONSOLE_WIFI_STATUS_BAR 1
#elif FEATURE_ENABLED(PICO_CALC) && FEATURE_ENABLED(DISPLAY) && defined(LEXXOS_PLATFORM_PICO) && defined(LEXXOS_PICO_CYW43_AVAILABLE) && defined(LEXXOS_PICO_CYW43_LWIP_AVAILABLE)
	#include <lexxos/apps/network/detail/wifi_program_support.h>
	#define LEXXOS_LCD_CONSOLE_WIFI_STATUS_BAR 1
#else
	#define LEXXOS_LCD_CONSOLE_WIFI_STATUS_BAR 0
#endif

namespace {

std::atomic_bool g_runtimeBootComplete{false};
constexpr std::uint8_t unappliedFocusTarget = 0xFFU;
constexpr std::uint32_t unappliedFocusProcessId = static_cast<std::uint32_t>(-1);
std::atomic<std::uint8_t> g_appliedFocusTarget{unappliedFocusTarget};
std::atomic<std::uint32_t> g_appliedFocusProcessId{unappliedFocusProcessId};

constexpr std::uint32_t consoleTaskStackSizeBytes =
    lexxos::runtime::standardRuntimeProfile.taskStacks.consoleTaskBytes;
constexpr std::uint32_t systemEventStackSizeBytes =
    lexxos::runtime::standardRuntimeProfile.taskStacks.systemEventTaskBytes;

#if FEATURE_ENABLED(HEALTH)
constexpr std::uint32_t healthTaskStackSizeBytes =
    lexxos::runtime::standardRuntimeProfile.taskStacks.healthTaskBytes;
#endif

#if FEATURE_ENABLED(DEBUG_DISPLAY)
constexpr std::uint32_t debugDisplayTaskStackSizeBytes =
    lexxos::runtime::standardRuntimeProfile.taskStacks.debugDisplayTaskBytes;
#endif

#if FEATURE_ENABLED(DISPLAY)
	#if FEATURE_ENABLED(PICO_CALC) && defined(PICO_RP2350) && PICO_RP2350
// PicoCalc ILI9488 text rendering uses wider RGB888 transfer buffers and nested
// display/resource-lock calls. Keep a larger stack margin for Debug builds and
// for stack high-water diagnostics after large console dumps such as `help`.
constexpr std::uint32_t consoleMirrorStackSizeBytes = 8192;
	#elif LEXXOS_LCD_CONSOLE_TARGET_ENABLED && defined(LEXXOS_PLATFORM_ESPIDF)
constexpr std::uint32_t consoleMirrorStackSizeBytes = 8192;
	#else
constexpr std::uint32_t consoleMirrorStackSizeBytes =
    lexxos::runtime::standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes;
	#endif
constexpr std::uint32_t consoleMirrorFlushIntervalMs = 8;
constexpr std::uint32_t startupConsoleMirrorFlushPasses = 48;
constexpr std::uint32_t lcdConsoleMirrorTaskPriority = 2;
constexpr std::uint32_t displayPowerTaskStackSizeBytes = 3072;
constexpr std::uint32_t displayPowerTaskPollIntervalMs = 50;
constexpr std::string_view displayPowerPointerOwner = "display-power-pointer";

	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
constexpr std::uint32_t lcdConsoleStatusBarPollIntervalMs = 1000;
	#endif

	#if FEATURE_ENABLED(PICO_CALC)
constexpr std::string_view picoCalcStatusBarOwner = "picocalc-status";
	#endif

#endif

void writeConsole(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	lexxos::app::console_output::write(platform, text);
}

void writeConsoleLine(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	writeConsole(platform, text);
	writeConsole(platform, "\r\n");
}

void flushConsoleMirrorForStartup() {
#if FEATURE_ENABLED(DISPLAY)
	if (lexxos::app::console_mirror::mode() != lexxos::app::ConsoleMirrorMode::Text) {
		return;
	}
	for (std::uint32_t pass = 0; pass < startupConsoleMirrorFlushPasses; ++pass) {
		lexxos::app::console_mirror::flush();
		lexxos::app::console_mirror::refresh();
		lexxos::kernel::sleepMs(1);
	}
#endif
}

void resetConsoleForShellStartup(lexxos::platform::PlatformServices const& platform) {
#if FEATURE_ENABLED(DISPLAY)
	lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::Shell);
	lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
	lexxos::app::console_output::clear(platform);
#else
	(void) platform;
#endif
}

lexxos::platform::PlatformServices const& platformServices(lexxos::app::RuntimeTaskContext const& context) {
	if (context.operatingSystem != nullptr) {
		return context.operatingSystem->platform();
	}
	return lexxos::platform::services();
}

std::uint32_t platformUptimeMs(lexxos::platform::PlatformServices const& platform) {
	auto const& clock = platform.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

bool hasRuntimeCapability(lexxos::app::RuntimeTaskContext const& context, lexxos::runtime::Capability capability) {
	return context.operatingSystem != nullptr && context.operatingSystem->capabilities().has(capability);
}

lexxos::platform::BoardDescriptor const& activeBoard(lexxos::app::RuntimeTaskContext const& context) {
	auto const& platform = platformServices(context);
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

#if defined(LEXXOS_PLATFORM_PICO) || defined(LEXXOS_PLATFORM_STM32F4)
void discardPendingConsoleInput(lexxos::platform::PlatformServices const& platform) {
	lexxos::app::console_input::clearInjected();
	if (platform.console.readChar == nullptr) {
		return;
	}

	char ignored{};
	while (platform.console.readChar(ignored)) {
	}
}
#endif

std::size_t readPhysicalConsoleInput(
    lexxos::platform::PlatformServices const& platform,
    char* buffer,
    std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}
	if (platform.console.readAvailable != nullptr) {
		return platform.console.readAvailable(buffer, capacity);
	}
	if (platform.console.readChar != nullptr && platform.console.readChar(buffer[0])) {
		return 1;
	}
	return 0;
}

void applyTaskFocus(lexxos::program::ProgramFocusSnapshot const& focus) {
	auto const target = static_cast<std::uint8_t>(focus.target);
	auto const previousTarget = g_appliedFocusTarget.load(std::memory_order_acquire);
	auto const previousProcessId = g_appliedFocusProcessId.load(std::memory_order_acquire);
	auto const expectedInputMode = focus.programFocused()
	    ? lexxos::app::console_input::Mode::Program
	    : lexxos::app::console_input::Mode::Shell;
	auto const currentInputMode = lexxos::app::console_input::mode();
	auto const inputModeMatches = currentInputMode == expectedInputMode
	    || (focus.programFocused() && currentInputMode == lexxos::app::console_input::Mode::RawProgram);
#if FEATURE_ENABLED(DISPLAY)
	auto const expectedMirrorMode = focus.graphicalProgramFocused()
	    ? lexxos::app::ConsoleMirrorMode::Graphics
	    : lexxos::app::ConsoleMirrorMode::Text;
	auto const mirrorModeMatches = lexxos::app::console_mirror::mode() == expectedMirrorMode;
	if (previousTarget == target && previousProcessId == focus.processId && inputModeMatches && mirrorModeMatches) {
		return;
	}
#else
	if (previousTarget == target && previousProcessId == focus.processId && inputModeMatches) {
		return;
	}
#endif

	g_appliedFocusTarget.store(target, std::memory_order_release);
	g_appliedFocusProcessId.store(focus.processId, std::memory_order_release);
	lexxos::app::console_input::setMode(expectedInputMode);

#if FEATURE_ENABLED(DISPLAY)
	lexxos::app::console_mirror::setMode(expectedMirrorMode);
	if (expectedMirrorMode == lexxos::app::ConsoleMirrorMode::Text) {
		lexxos::app::console_mirror::refresh();
	}
#else
	(void) focus;
#endif
}

lexxos::program::ProgramFocusSnapshot currentTaskFocus() {
	auto focus = lexxos::program::supervisor().focus();
	applyTaskFocus(focus);
	return focus;
}

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	#if FEATURE_ENABLED(PICO_CALC) || FEATURE_ENABLED(CARDPUTER)
std::uint8_t normalizeStatusBarBatteryPercent(std::uint8_t value) {
	return value > 100 ? 100 : value;
}
	#endif

bool lcdConsoleStatusBarPollDue(lexxos::app::RuntimeTaskContext& context, std::uint32_t& now) {
	if (context.operatingSystem == nullptr) {
		return false;
	}

	static std::uint32_t nextStatusPollMs = 0;
	now = platformUptimeMs(platformServices(context));
	if (nextStatusPollMs != 0 && static_cast<std::int32_t>(now - nextStatusPollMs) < 0) {
		return false;
	}
	nextStatusPollMs = now + lcdConsoleStatusBarPollIntervalMs;
	return true;
}

	#if FEATURE_ENABLED(PICO_CALC)
void updatePicoCalcPowerStatus(
    lexxos::app::RuntimeTaskContext& context,
    lexxos::app::picocalc_lcd_console::StatusBarInfo& status,
    std::uint32_t now) {
	auto& devices = context.operatingSystem->devices();
	auto* service = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(devices);
	if (service == nullptr || !service->isReady()) {
		return;
	}

	auto serviceLock = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
	    devices,
	    picoCalcStatusBarOwner,
	    lexxos::devices::DeviceResourcePriority::Background);

	lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
	bool hasSnapshot = service->southbridgeSnapshot(snapshot);
	if (serviceLock && (!hasSnapshot || !snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validBattery))) {
		hasSnapshot = service->refreshSouthbridgeSnapshot(now) && service->southbridgeSnapshot(snapshot);
	}

	if (hasSnapshot && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validBattery)) {
		status.batteryValid = true;
		status.batteryCharging = snapshot.battery.charging();
		status.batteryPercent = normalizeStatusBarBatteryPercent(snapshot.battery.value());
	}
		#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	if (hasSnapshot && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validJcsRtcDateTime)) {
		status.timeValid = true;
		status.hour = snapshot.jcsRtcDateTime.hour;
		status.minute = snapshot.jcsRtcDateTime.minute;
		status.second = snapshot.jcsRtcDateTime.second;
	}
		#endif
}
	#endif

	#if FEATURE_ENABLED(CARDPUTER)
void updateCardputerPowerStatus(
    lexxos::app::RuntimeTaskContext& context,
    lexxos::app::picocalc_lcd_console::StatusBarInfo& status) {
	auto const& power = platformServices(context).power;
	if (power.readBattery == nullptr) {
		return;
	}

	lexxos::platform::PlatformBatteryStatus battery{};
	if (!power.readBattery(battery) || !battery.available || !battery.percentValid) {
		return;
	}

	status.batteryValid = true;
	status.batteryPercent = normalizeStatusBarBatteryPercent(battery.percent);
	status.batteryCharging = battery.chargingValid && battery.charging;
}
	#endif

void updateLcdConsoleSoftwareRtcStatus(
    lexxos::app::RuntimeTaskContext& context,
    lexxos::app::picocalc_lcd_console::StatusBarInfo& status) {
	auto const rtc = lexxos::devices::rtc::software::status(platformServices(context));
	if (!rtc.configured || !rtc.valid) {
		return;
	}

	status.timeValid = true;
	status.hour = rtc.now.hour;
	status.minute = rtc.now.minute;
	status.second = rtc.now.second;
}

void updateLcdConsoleWifiStatus(
    lexxos::app::RuntimeTaskContext& context,
    lexxos::app::picocalc_lcd_console::StatusBarInfo& status) {
	#if LEXXOS_LCD_CONSOLE_WIFI_STATUS_BAR
	auto const& board = activeBoard(context);
	auto const wifi = lexxos::apps::network_programs::detail::readWifiRuntimeStatus(board);
	status.wifiValid = board.wireless.wifi;
	if (wifi.supported && wifi.initialized) {
		status.wifiValid = true;
		status.wifiConnected = wifi.connected;
		status.wifiSignalLevel = wifi.signalLevel;
	}
	#else
	(void) context;
	(void) status;
	#endif
}

void updateLcdConsoleStatusBar(lexxos::app::RuntimeTaskContext& context) {
	std::uint32_t now = 0;
	if (!lcdConsoleStatusBarPollDue(context, now)) {
		return;
	}

	lexxos::app::picocalc_lcd_console::StatusBarInfo status{};
	status.visible = true;
	status.updatedMs = now;

	#if FEATURE_ENABLED(PICO_CALC)
	updatePicoCalcPowerStatus(context, status, now);
	#endif

	#if FEATURE_ENABLED(CARDPUTER)
	updateCardputerPowerStatus(context, status);
	#endif

	if (!status.timeValid) {
		updateLcdConsoleSoftwareRtcStatus(context, status);
	}
	updateLcdConsoleWifiStatus(context, status);

	lexxos::app::picocalc_lcd_console::setStatusBarInfo(status);
}
#endif

#if LEXXOS_LOCAL_CONSOLE_INPUT
constexpr std::uint32_t picoCalcInputTaskStackSizeBytes = lexxos::runtime::standardRuntimeProfile.taskStacks.localConsoleInputTaskBytes;
constexpr std::uint32_t maxPicoCalcConsoleInputEventsPerPoll = 4;
constexpr std::uint32_t maxPicoCalcDiscardEvents = 8;
constexpr std::uint32_t picoCalcConsoleInputPollIntervalMs = 16;
constexpr int picoCalcScrollbackLineStep = 1;
constexpr int picoCalcScrollbackFallbackPageLines = 20;
constexpr int picoCalcHorizontalScrollColumnStep = 8;
constexpr int picoCalcHorizontalScrollFallbackPageColumns = 32;
	#if FEATURE_ENABLED(CARDPUTER)
constexpr std::string_view picoCalcKeyboardDeviceName = "cardputer-keyboard";
constexpr std::string_view picoCalcConsoleInputOwner = "cardputer-console-input";
	#elif LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1
constexpr std::string_view picoCalcKeyboardDeviceName = "usb-keyboard";
constexpr std::string_view picoCalcConsoleInputOwner = "usb-console-input";
	#else
constexpr std::string_view picoCalcKeyboardDeviceName = "picocalc-keyboard";
constexpr std::string_view picoCalcConsoleInputOwner = "console-input";
	#endif

std::uint32_t picoCalcUptimeMs(lexxos::platform::PlatformServices const& platform) {
	return platformUptimeMs(platform);
}

bool timeReached(std::uint32_t now, std::uint32_t target) {
	return static_cast<std::int32_t>(now - target) >= 0;
}

bool injectConsoleInput(std::string_view text) {
	return lexxos::app::console_input::inject(text);
}

bool injectConsoleEscapeSequence(char finalByte) {
	char sequence[] = {'\x1B', '[', finalByte};
	return injectConsoleInput({sequence, sizeof(sequence)});
}

bool injectConsoleDeleteSequence() {
	return injectConsoleInput("\x1B[3~");
}

bool injectConsoleCharacter(lexxos::devices::InputEvent const& event) {
	if (event.character == 0) {
		return false;
	}

	char value = static_cast<char>(event.character);
	if ((event.modifiers & lexxos::devices::InputModifierCtrl) != 0) {
		if (value >= 'a' && value <= 'z') {
			value = static_cast<char>(value - 'a' + 1);
		} else if (value >= 'A' && value <= 'Z') {
			value = static_cast<char>(value - 'A' + 1);
		}
	}

	char const sequence[] = {value};
	return injectConsoleInput({sequence, sizeof(sequence)});
}

bool hasShiftModifier(lexxos::devices::InputEvent const& event) {
	return (event.modifiers & (lexxos::devices::InputModifierShiftLeft | lexxos::devices::InputModifierShiftRight)) != 0;
}

bool hasSymModifier(lexxos::devices::InputEvent const& event) {
	return (event.modifiers & lexxos::devices::InputModifierSym) != 0;
}

bool hasAltModifier(lexxos::devices::InputEvent const& event) {
	return (event.modifiers & lexxos::devices::InputModifierAlt) != 0;
}

bool hasCtrlModifier(lexxos::devices::InputEvent const& event) {
	return (event.modifiers & lexxos::devices::InputModifierCtrl) != 0;
}

bool isTaskSwitchInput(lexxos::devices::InputEvent const& event) {
	return event.type == lexxos::devices::InputEventType::KeyDown
	    && event.key == lexxos::devices::KeyCode::Tab
	    && hasAltModifier(event);
}

bool handleTaskSwitchInput(lexxos::devices::InputEvent const& event, lexxos::program::ProgramFocusSnapshot& focus) {
	if (!isTaskSwitchInput(event)) {
		return false;
	}
	focus = lexxos::program::supervisor().focusNext();
	applyTaskFocus(focus);
	return true;
}

	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
int picoCalcScrollbackPageLines() {
	auto const info = lexxos::app::picocalc_lcd_console::info();
	if (info.ready && info.rows > 2) {
		return info.rows - 1;
	}
	return picoCalcScrollbackFallbackPageLines;
}

int picoCalcHorizontalScrollPageColumns() {
	auto const info = lexxos::app::picocalc_lcd_console::info();
	if (info.ready && info.columns > 2) {
		return info.columns - 1;
	}
	return picoCalcHorizontalScrollFallbackPageColumns;
}
	#endif

bool handleConsoleScrollbackInput(lexxos::devices::InputEvent const& event) {
	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	if (event.type != lexxos::devices::InputEventType::KeyDown) {
		return false;
	}

	switch (event.key) {
		case lexxos::devices::KeyCode::Up:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackUp(picoCalcScrollbackLineStep);
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackUp(picoCalcScrollbackPageLines());
				return true;
			}
			break;
		case lexxos::devices::KeyCode::Down:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackDown(picoCalcScrollbackLineStep);
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackDown(picoCalcScrollbackPageLines());
				return true;
			}
			break;
		case lexxos::devices::KeyCode::Left:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackLeft(picoCalcHorizontalScrollColumnStep);
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackLeft(picoCalcHorizontalScrollPageColumns());
				return true;
			}
			break;
		case lexxos::devices::KeyCode::Right:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackRight(picoCalcHorizontalScrollColumnStep);
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackRight(picoCalcHorizontalScrollPageColumns());
				return true;
			}
			break;
		case lexxos::devices::KeyCode::Home:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackLeftEdge();
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackHome();
				return true;
			}
			break;
		case lexxos::devices::KeyCode::End:
			if (hasShiftModifier(event) || hasCtrlModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackRightEdge();
				return true;
			}
			if (hasSymModifier(event) || hasAltModifier(event)) {
				lexxos::app::picocalc_lcd_console::scrollbackEnd();
				return true;
			}
			break;
		case lexxos::devices::KeyCode::Escape:
			if (lexxos::app::picocalc_lcd_console::isScrollbackActive()) {
				lexxos::app::picocalc_lcd_console::scrollbackResetView();
				return true;
			}
			break;
		default:
			break;
	}
	#else
	(void) event;
	#endif
	return false;
}

void leaveConsoleScrollbackForInput(lexxos::devices::InputEvent const& event) {
	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	if (event.type == lexxos::devices::InputEventType::KeyDown && lexxos::app::picocalc_lcd_console::isScrollbackActive()) {
		lexxos::app::picocalc_lcd_console::scrollbackResetView();
	}
	#else
	(void) event;
	#endif
}

bool injectPicoCalcInputEvent(lexxos::devices::InputEvent const& event) {
	if (event.type != lexxos::devices::InputEventType::KeyDown) {
		return false;
	}

	if (handleConsoleScrollbackInput(event)) {
		return true;
	}
	leaveConsoleScrollbackForInput(event);

	switch (event.key) {
		case lexxos::devices::KeyCode::Up:
			return injectConsoleEscapeSequence('A');
		case lexxos::devices::KeyCode::Down:
			return injectConsoleEscapeSequence('B');
		case lexxos::devices::KeyCode::Right:
			return injectConsoleEscapeSequence('C');
		case lexxos::devices::KeyCode::Left:
			return injectConsoleEscapeSequence('D');
		case lexxos::devices::KeyCode::Home:
			return injectConsoleEscapeSequence('H');
		case lexxos::devices::KeyCode::End:
			return injectConsoleEscapeSequence('F');
		case lexxos::devices::KeyCode::Delete:
			return injectConsoleDeleteSequence();
		case lexxos::devices::KeyCode::Backspace:
			return injectConsoleInput("\b");
		case lexxos::devices::KeyCode::Escape:
			if (lexxos::app::console_input::mode() == lexxos::app::console_input::Mode::Shell) {
				return injectConsoleInput("\x03");
			}
			return injectConsoleInput("\x1B");
		case lexxos::devices::KeyCode::Break:
			return injectConsoleInput("\x03");
		default:
			break;
	}

	return injectConsoleCharacter(event);
}

bool wakePicoCalcDisplayForInput(lexxos::app::RuntimeTaskContext& context, lexxos::devices::InputEvent const& event) {
	#if FEATURE_ENABLED(DISPLAY)
	if (event.type != lexxos::devices::InputEventType::KeyDown
	    || !hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayRgb565)
	    || !hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayPowerMode)) {
		return false;
	}

	return lexxos::app::picocalc_display_power::wakeForInput(
	    context.operatingSystem->devices(),
	    platformServices(context));
	#else
	(void) context;
	(void) event;
	return false;
	#endif
}

void recordPicoCalcDisplayActivity(lexxos::app::RuntimeTaskContext& context, lexxos::devices::InputEvent const& event) {
	#if FEATURE_ENABLED(DISPLAY)
	if (event.type == lexxos::devices::InputEventType::KeyDown
	    && hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayRgb565)) {
		lexxos::app::picocalc_display_power::recordActivity(platformServices(context));
	}
	#else
	(void) context;
	(void) event;
	#endif
}

void recordPicoCalcDisplayConsoleActivity(lexxos::os::AppContext& context) {
	#if FEATURE_ENABLED(DISPLAY)
	if (context.capabilities().has(lexxos::runtime::Capability::DisplayRgb565)) {
		lexxos::app::picocalc_display_power::recordActivity(context.platform());
		if (context.capabilities().has(lexxos::runtime::Capability::DisplayPowerMode)) {
			(void) lexxos::app::picocalc_display_power::wakeForInput(context.devices(), context.platform(), "display-console-wake");
		}
	}
	#else
	(void) context;
	#endif
}

lexxos::devices::InputDevice* picoCalcKeyboard(lexxos::devices::DeviceManager& devices) {
	auto* input = devices.inputDevice(picoCalcKeyboardDeviceName);
	if (input == nullptr || !input->isReady()) {
		return nullptr;
	}
	return input;
}

lexxos::devices::DeviceResourceLock lockPicoCalcConsoleInput(lexxos::devices::DeviceManager& devices) {
	return devices.lockInputDevice(
	    picoCalcKeyboardDeviceName,
	    lexxos::devices::DeviceResourcePriority::Normal,
	    picoCalcConsoleInputOwner);
}

bool pollPicoCalcConsoleInput(lexxos::app::RuntimeTaskContext& context) {
	if (context.operatingSystem == nullptr) {
		return false;
	}
	if (!hasRuntimeCapability(context, lexxos::runtime::Capability::InputKeyboard)) {
		return false;
	}

	auto& devices = context.operatingSystem->devices();
	auto* input = picoCalcKeyboard(devices);
	if (input == nullptr) {
		return false;
	}

	auto inputLock = lockPicoCalcConsoleInput(devices);
	if (!inputLock) {
		return false;
	}

	static std::uint32_t nextPollMs = 0;
	auto const now = picoCalcUptimeMs(platformServices(context));
	if (nextPollMs != 0 && !timeReached(now, nextPollMs)) {
		return false;
	}
	nextPollMs = now + picoCalcConsoleInputPollIntervalMs;

	bool consumed = false;
	lexxos::devices::InputEvent event;
	for (std::uint32_t index = 0; index < maxPicoCalcConsoleInputEventsPerPoll; ++index) {
		if (!input->pollEvent(event)) {
			break;
		}
		recordPicoCalcDisplayActivity(context, event);
		if (wakePicoCalcDisplayForInput(context, event)) {
			consumed = true;
			continue;
		}

		auto focus = currentTaskFocus();
		if (handleTaskSwitchInput(event, focus)) {
			consumed = true;
			continue;
		}

		if (focus.programFocused()) {
			lexxos::app::program_input::publish(event);
			// Keep console_input fed for terminal-style foreground programs, but
			// shellProgram will not read it while a program owns focus.
			consumed = injectPicoCalcInputEvent(event) || consumed;
			continue;
		}

		consumed = injectPicoCalcInputEvent(event) || consumed;
	}
	return consumed;
}

void discardPendingPicoCalcShellInput(lexxos::os::AppContext& context) {
	if (!context.capabilities().has(lexxos::runtime::Capability::InputKeyboard)) {
		return;
	}

	auto& devices = context.devices();
	auto* input = picoCalcKeyboard(devices);
	if (input == nullptr) {
		return;
	}

	auto inputLock = lockPicoCalcConsoleInput(devices);
	if (!inputLock) {
		return;
	}

	lexxos::devices::InputEvent ignored;
	for (std::uint32_t index = 0; index < maxPicoCalcDiscardEvents; ++index) {
		if (!input->pollEvent(ignored)) {
			return;
		}
	}
}

void picoCalcInputTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	while (!lexxos::kernel::isShutdownRequested()) {
		auto const consumed = pollPicoCalcConsoleInput(runtimeContext);
		if (consumed) {
			lexxos::kernel::yield();
		} else {
			lexxos::kernel::sleepMs(1);
		}
	}
}
#endif

int shellProgram(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&) {
	auto* shellPointer = context.shell();
	if (shellPointer == nullptr) {
		context.writeLine("shell: missing shell context");
		return 1;
	}
	auto& shell = *shellPointer;
	currentTaskFocus();
	resetConsoleForShellStartup(context.platform());
	shell.printBanner();

	if (context.owner() == lexxos::os::AppContextOwner::Shell) {
		flushConsoleMirrorForStartup();
		return 0;
	}

#if defined(LEXXOS_PLATFORM_PICO) || defined(LEXXOS_PLATFORM_STM32F4)
	discardPendingConsoleInput(context.platform());
#endif

#if LEXXOS_LOCAL_CONSOLE_INPUT
	discardPendingPicoCalcShellInput(context);
#endif

	shell.printPrompt();
	flushConsoleMirrorForStartup();
	while (!shell.shouldStop() && !lexxos::kernel::isShutdownRequested()) {
		bool consumedInput = false;
		auto const focus = currentTaskFocus();
		if (focus.programFocused()) {
			// A foreground program owns the terminal input. Do not let the shell
			// drain console_input while that program is focused; otherwise keys that
			// were injected for terminal-style foreground programs can be consumed by
			// the shell before focus returns.
			std::array<char, lexxos::runtime::standardRuntimeProfile.shell.maxLineLength> inputBatch{};
			auto const inputCount = readPhysicalConsoleInput(context.platform(), inputBatch.data(), inputBatch.size());
			if (inputCount != 0) {
#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
				recordPicoCalcDisplayConsoleActivity(context);
#endif
				shell.feed(std::string_view(inputBatch.data(), inputCount));
				lexxos::kernel::yield();
				continue;
			}
			lexxos::kernel::sleepMs(1);
			continue;
		}
		shell.printPromptIfPending();

		std::array<char, lexxos::runtime::standardRuntimeProfile.shell.maxLineLength> inputBatch{};
		auto const inputCount = lexxos::app::console_input::readAvailable(context.platform(), inputBatch.data(), inputBatch.size());
		if (inputCount != 0) {
#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
			recordPicoCalcDisplayConsoleActivity(context);
#endif
			shell.feed(std::string_view(inputBatch.data(), inputCount));
			consumedInput = true;
		}
		if (!consumedInput) {
			lexxos::kernel::sleepMs(1);
		} else {
			lexxos::kernel::yield();
		}
	}
	lexxos::kernel::requestShutdown();
	return 0;
}

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
void finishLcdConsoleBootScreen(lexxos::app::RuntimeTaskContext& runtimeContext) {
	g_runtimeBootComplete.store(true, std::memory_order_release);
	updateLcdConsoleStatusBar(runtimeContext);
	lexxos::app::picocalc_lcd_console::refreshStatusBar();
	lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::Shell);
	lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
}
#endif

void consoleTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	auto const& platform = platformServices(runtimeContext);
	if (runtimeContext.operatingSystem == nullptr) {
		writeConsoleLine(platform, "console task: missing operating system");
		lexxos::kernel::requestShutdown();
		return;
	}
	if (runtimeContext.shell == nullptr) {
		writeConsoleLine(platform, "console task: missing shell");
		lexxos::kernel::requestShutdown();
		return;
	}

	auto& shell = *runtimeContext.shell;

#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
	if (hasRuntimeCapability(runtimeContext, lexxos::runtime::Capability::DisplayRgb565)) {
		finishLcdConsoleBootScreen(runtimeContext);
	}
#endif

	auto context = runtimeContext.operatingSystem->createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	    .shell = &shell,
	    .workingDirectory = shell.workingDirectory(),
	});

	char const* argv[] = {"shell"};
	auto const result = context.launch({
	    .name = "shell",
	    .argc = 1,
	    .argv = argv,
	});
	if (result.launchStatus != lexxos::program::ProgramLaunchStatus::Available) {
		writeConsole(platform, "failed to launch shell program: ");
		writeConsole(platform, lexxos::program::toString(result.launchStatus));
		writeConsole(platform, "\r\n");
	}
	lexxos::kernel::requestShutdown();
}

#if FEATURE_ENABLED(HEALTH)
void healthTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	auto const& gpio = platformServices(runtimeContext).gpio;
	if (gpio.setStatusLed == nullptr) {
		return;
	}

	bool ledEnabled = false;
	while (!lexxos::kernel::isShutdownRequested()) {
		ledEnabled = !ledEnabled;
		gpio.setStatusLed(ledEnabled);
		lexxos::kernel::sleepMs(500);
	}
	gpio.setStatusLed(false);
}
#endif

void systemEventTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	if (runtimeContext.systemEvents == nullptr) {
		return;
	}
	runtimeContext.systemEvents->run();
}

#if FEATURE_ENABLED(DISPLAY)
struct PointerActivitySnapshot {
	bool valid{false};
	std::uint32_t reportCount{0};
	std::int32_t x{0};
	std::int32_t y{0};
	std::int32_t wheel{0};
	std::uint8_t buttons{0};
};

std::array<PointerActivitySnapshot, lexxos::devices::DeviceManager::maxPointingDevices> g_pointerActivitySnapshots{};

bool pointerStateChanged(
    PointerActivitySnapshot const& previous,
    lexxos::devices::PointingDeviceState const& state) {
	if (!previous.valid) {
		return false;
	}
	return state.reportCount != previous.reportCount
	    && (state.x != previous.x
	        || state.y != previous.y
	        || state.wheel != previous.wheel
	        || state.buttons != previous.buttons
	        || state.buttons != 0);
}

void updatePointerSnapshot(
    PointerActivitySnapshot& snapshot,
    lexxos::devices::PointingDeviceState const& state) {
	snapshot.valid = true;
	snapshot.reportCount = state.reportCount;
	snapshot.x = state.x;
	snapshot.y = state.y;
	snapshot.wheel = state.wheel;
	snapshot.buttons = state.buttons;
}

void pollDisplayPointerActivity(lexxos::app::RuntimeTaskContext& runtimeContext) {
	if (runtimeContext.operatingSystem == nullptr
	    || !hasRuntimeCapability(runtimeContext, lexxos::runtime::Capability::InputPointer)) {
		return;
	}

	auto& devices = runtimeContext.operatingSystem->devices();
	auto const count = devices.pointingDeviceCount();
	for (std::size_t index = 0; index < count && index < g_pointerActivitySnapshots.size(); ++index) {
		auto const& descriptor = devices.pointingDeviceAt(index);
		auto* pointer = descriptor.device;
		if (pointer == nullptr || !pointer->isReady()) {
			continue;
		}

		auto lock = devices.lockPointingDevice(
		    descriptor.name,
		    lexxos::devices::DeviceResourcePriority::Background,
		    displayPowerPointerOwner);
		if (!lock) {
			continue;
		}

		auto const state = pointer->state();
		auto& snapshot = g_pointerActivitySnapshots[index];
		bool const changed = pointerStateChanged(snapshot, state);
		updatePointerSnapshot(snapshot, state);
		if (!changed) {
			continue;
		}

		auto const& platform = platformServices(runtimeContext);
		lexxos::app::picocalc_display_power::recordActivity(platform);
		if (hasRuntimeCapability(runtimeContext, lexxos::runtime::Capability::DisplayPowerMode)) {
			(void) lexxos::app::picocalc_display_power::wakeForInput(devices, platform, "display-pointer-wake");
		}
	}
}

bool displayPowerPollDue(lexxos::app::RuntimeTaskContext& runtimeContext) {
	static std::uint32_t nextDisplayPowerPollMs = 0;
	auto const now = platformUptimeMs(platformServices(runtimeContext));
	if (nextDisplayPowerPollMs != 0 && static_cast<std::int32_t>(now - nextDisplayPowerPollMs) < 0) {
		return false;
	}
	nextDisplayPowerPollMs = now + displayPowerTaskPollIntervalMs;
	return true;
}

void updateDisplayPowerIdle(lexxos::app::RuntimeTaskContext& runtimeContext) {
	if (runtimeContext.operatingSystem == nullptr
	    || !hasRuntimeCapability(runtimeContext, lexxos::runtime::Capability::DisplayRgb565)
	    || !hasRuntimeCapability(runtimeContext, lexxos::runtime::Capability::DisplayPowerMode)
	    || !displayPowerPollDue(runtimeContext)) {
		return;
	}

	pollDisplayPointerActivity(runtimeContext);
	(void) lexxos::app::picocalc_display_power::updateIdle(
	    runtimeContext.operatingSystem->devices(),
	    platformServices(runtimeContext));
}

void displayPowerTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	while (!lexxos::kernel::isShutdownRequested()) {
		updateDisplayPowerIdle(runtimeContext);
		lexxos::kernel::sleepMs(displayPowerTaskPollIntervalMs);
	}
}

void consoleMirrorTask(void* userData) {
	auto* runtimeContext = static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	while (!lexxos::kernel::isShutdownRequested()) {
		if (lexxos::app::console_mirror::mode() == lexxos::app::ConsoleMirrorMode::Text) {
			lexxos::app::console_mirror::flush();
		}
		if (runtimeContext != nullptr) {
			updateDisplayPowerIdle(*runtimeContext);
		}
	#if LEXXOS_LCD_CONSOLE_TARGET_ENABLED
		if (runtimeContext != nullptr
		    && g_runtimeBootComplete.load(std::memory_order_acquire)
		    && hasRuntimeCapability(*runtimeContext, lexxos::runtime::Capability::DisplayRgb565)) {
			updateLcdConsoleStatusBar(*runtimeContext);
		}
	#endif
		lexxos::kernel::sleepMs(consoleMirrorFlushIntervalMs);
	}
}
#endif

#if FEATURE_ENABLED(DEBUG_DISPLAY)
void debugDisplayTask(void* userData) {
	auto& runtimeContext = *static_cast<lexxos::app::RuntimeTaskContext*>(userData);
	if (runtimeContext.debugDisplay == nullptr) {
		return;
	}
	auto& debugDisplay = *runtimeContext.debugDisplay;

	debugDisplay.drawStatic();
	while (!lexxos::kernel::isShutdownRequested()) {
		debugDisplay.refresh();
		lexxos::kernel::sleepMs(1000);
	}
}
#endif

} // namespace

void lexxos::app::registerShellProgram(lexxos::program::ProgramManager& programs) {
	programs.registerProgram({
	    .name = "shell",
	    .help = "start interactive LexxOS shell",
	    .description = "Starts the interactive LexxOS shell. When it is called from an already running shell, it prints the banner and returns.",
	    .usage = "shell",
	    .handler = shellProgram,
	    .legacy = false,
	    .enabled = true,
	});
}

bool lexxos::app::startRuntimeTasks(RuntimeTaskContext& context) {
	auto const& platform = platformServices(context);
	bool displayPowerHandledByConsoleMirror = false;

#if FEATURE_ENABLED(DISPLAY)
	// Start the mirror before the interactive console. On preemptive targets the
	// console task has a higher priority and can otherwise print the boot shell
	// banner/prompt before the LCD mirror task exists, leaving only the renderer's
	// initial blank/cursor frame visible on the device display.
	if (hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayRgb565)
	    && lexxos::app::console_mirror::isReady()
	    && !lexxos::kernel::createTask(consoleMirrorTask, &context, {
	                                                                    .name = "console-mirror",
	                                                                    .stackSizeBytes = consoleMirrorStackSizeBytes,
	                                                                    .priority = lcdConsoleMirrorTaskPriority,
	                                                                    .executionClass = lexxos::kernel::TaskExecutionClass::Driver,
	                                                                })) {
		writeConsoleLine(platform, "failed to create console mirror task");
		return false;
	}
	displayPowerHandledByConsoleMirror =
	    hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayRgb565)
	    && lexxos::app::console_mirror::isReady();
#endif

	if (!lexxos::kernel::createTask(consoleTask, &context, {
	                                                           .name = "console",
	                                                           .stackSizeBytes = consoleTaskStackSizeBytes,
	                                                           .priority = 2,
	                                                           .executionClass = lexxos::kernel::TaskExecutionClass::System,
	                                                       })) {
		writeConsoleLine(platform, "failed to create console task");
		return false;
	}

#if LEXXOS_LOCAL_CONSOLE_INPUT
	if (hasRuntimeCapability(context, lexxos::runtime::Capability::InputKeyboard)
	    && !lexxos::kernel::createTask(picoCalcInputTask, &context, {
	                                                                    .name =
	#if FEATURE_ENABLED(CARDPUTER)
	                                                                        "cardputer-input",
	#elif LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1
	                                                                        "usb-hid-input",
	#else
	                                                                        "picocalc-input",
	#endif
	                                                                    .stackSizeBytes = picoCalcInputTaskStackSizeBytes,
	                                                                    .priority = 1,
	                                                                    .executionClass = lexxos::kernel::TaskExecutionClass::Driver,
	                                                                })) {
		writeConsoleLine(platform, "failed to create picocalc input task");
		return false;
	}
#endif

#if FEATURE_ENABLED(DISPLAY)
	if (!displayPowerHandledByConsoleMirror
	    && hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayRgb565)
	    && hasRuntimeCapability(context, lexxos::runtime::Capability::DisplayPowerMode)
	    && !lexxos::kernel::createTask(displayPowerTask, &context, {
	                                                                   .name = "display-power",
	                                                                   .stackSizeBytes = displayPowerTaskStackSizeBytes,
	                                                                   .priority = 1,
	                                                                   .executionClass = lexxos::kernel::TaskExecutionClass::Background,
	                                                               })) {
		writeConsoleLine(platform, "failed to create display power task");
		return false;
	}
#endif

#if FEATURE_ENABLED(HEALTH)
	if (!lexxos::kernel::createTask(healthTask, &context, {
	                                                          .name = "health",
	                                                          .stackSizeBytes = healthTaskStackSizeBytes,
	                                                          .priority = 1,
	                                                          .executionClass = lexxos::kernel::TaskExecutionClass::Background,
	                                                      })) {
		writeConsoleLine(platform, "failed to create health task");
		return false;
	}
#endif

	if (!lexxos::kernel::createTask(systemEventTask, &context, {
	                                                               .name = "system-events",
	                                                               .stackSizeBytes = systemEventStackSizeBytes,
	                                                               .priority = 1,
	                                                               .executionClass = lexxos::kernel::TaskExecutionClass::System,
	                                                           })) {
		writeConsoleLine(platform, "failed to create system event task");
		return false;
	}

	if (!lexxos::app::startSoftwareRtcTask(context)) {
		writeConsoleLine(platform, "failed to create software RTC task");
		return false;
	}

	lexxos::app::startWifiAutoconnectTask(context);

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	if (context.debugDisplayReady && !lexxos::kernel::createTask(debugDisplayTask, &context, {
	                                                                                             .name = "display",
	                                                                                             .stackSizeBytes = debugDisplayTaskStackSizeBytes,
	                                                                                             .priority = 1,
	                                                                                             .executionClass = lexxos::kernel::TaskExecutionClass::Driver,
	                                                                                         })) {
		writeConsoleLine(platform, "failed to create display task");
		if (context.debugDisplay != nullptr) {
			context.debugDisplay->showTaskFailure();
		}
	}
#endif

	return true;
}
