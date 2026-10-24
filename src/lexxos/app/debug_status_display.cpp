#include <lexxos/app/debug_status_display.h>

#include <cstdint>
#include <string_view>

#include <lexxos/app/debug_status_display_types.h>
#include <lexxos/devices/debug_display.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(FATFS)
	#include <lexxos/fs/fat/fat_volume.h>
	#define LEXXOS_DEBUG_STATUS_HAS_FAT 1
#else
	#define LEXXOS_DEBUG_STATUS_HAS_FAT 0
#endif

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/app/sd_automount_service.h>
	#include <lexxos/devices/sd_card.h>
#endif

namespace debug_display = lexxos::devices::debug_display;

#if LEXXOS_DEBUG_STATUS_HAS_FAT
namespace fat = lexxos::fs::fat;
#endif

#if FEATURE_ENABLED(SD_CARD)
namespace sd_card = lexxos::devices::sd_card;
#endif

namespace {

using lexxos::app::detail::DisplayLine;

constexpr std::string_view debugI2cBusName = "debug";
constexpr std::string_view debugDisplayOwner = "debug-status";

lexxos::devices::DeviceResourceLock lockDebugDisplayBus(lexxos::os::OperatingSystem& operatingSystem) {
	return operatingSystem.devices().lockI2cBus(
	    debugI2cBusName,
	    lexxos::devices::DeviceResourcePriority::Background,
	    debugDisplayOwner);
}

void writeDisplayLine(std::uint8_t line, DisplayLine const& value) {
	debug_display::writeLine(line, value.view());
}

} // namespace

lexxos::app::DebugStatusDisplay::DebugStatusDisplay(lexxos::os::OperatingSystem& operatingSystem)
    : _operatingSystem(operatingSystem) {
}

bool lexxos::app::DebugStatusDisplay::initialize() {
	auto lock = lockDebugDisplayBus(_operatingSystem);
	if (!lock) {
		return false;
	}
	return debug_display::initialize(_operatingSystem.platform());
}

void lexxos::app::DebugStatusDisplay::drawStatic() {
	auto lock = lockDebugDisplayBus(_operatingSystem);
	if (!lock) {
		return;
	}

	auto const& board = _operatingSystem.platform().board;
	if (board.current == nullptr) {
		return;
	}
	auto const& descriptor = board.current();

	debug_display::clear();
	debug_display::writeLine(0, "LEXXOS DEBUG");
	debug_display::writeLine(1, descriptor.boardId);

	DisplayLine processorLine;
	processorLine.append("CPU ");
	processorLine.append(descriptor.processorName);
	writeDisplayLine(2, processorLine);

	DisplayLine uartLine;
	uartLine.append("UART");
	uartLine.appendUnsigned(static_cast<std::uint32_t>(descriptor.debugUart.index));
	uartLine.append(' ');
	uartLine.appendUnsigned(descriptor.debugUart.baudRate);
	writeDisplayLine(6, uartLine);

#if FEATURE_ENABLED(SD_CARD)
	debug_display::writeLine(7, "SD INIT...");
#else
	debug_display::writeLine(7, "SD DISABLED");
#endif
}

void lexxos::app::DebugStatusDisplay::showBooting() {
	auto lock = lockDebugDisplayBus(_operatingSystem);
	if (!lock) {
		return;
	}
	debug_display::writeLine(3, "BOOTING...");
}

void lexxos::app::DebugStatusDisplay::refresh() {
	auto lock = lockDebugDisplayBus(_operatingSystem);
	if (!lock) {
		return;
	}

	DisplayLine uptimeLine;
	uptimeLine.append("UP ");
	auto const& clock = _operatingSystem.platform().clock;
	uptimeLine.appendUnsigned(clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U);
	uptimeLine.append(" MS");
	writeDisplayLine(3, uptimeLine);

	auto const memory = lexxos::kernel::memorySnapshot();
	DisplayLine memoryLine;
	memoryLine.append("HEAP ");
	if (memory.available) {
		memoryLine.appendUnsigned(memory.freeHeapBytes);
		memoryLine.append(" B");
	} else {
		memoryLine.append("N/A");
	}
	writeDisplayLine(4, memoryLine);

	DisplayLine tasksLine;
	tasksLine.append("TASKS ");
	tasksLine.appendUnsigned(static_cast<std::uint32_t>(lexxos::kernel::taskCount()));
	writeDisplayLine(5, tasksLine);

	DisplayLine sdLine;
	sdLine.append("SD ");
#if FEATURE_ENABLED(SD_CARD)
	#if LEXXOS_DEBUG_STATUS_HAS_FAT
	if (fat::isMounted(SdAutomountService::mountPath)) {
		sdLine.append("mounted");
	} else
	#endif
	    if (sd_card::hasCardDetect() && !sd_card::isCardInserted()) {
		sdLine.append("empty");
	} else {
		sdLine.append(lexxos::devices::toString(sd_card::state()));
	}
#else
	sdLine.append("disabled");
#endif
	writeDisplayLine(7, sdLine);
}

void lexxos::app::DebugStatusDisplay::showTaskFailure() {
	auto lock = lockDebugDisplayBus(_operatingSystem);
	if (!lock) {
		return;
	}
	debug_display::writeLine(7, "DISPLAY TASK FAIL");
}
