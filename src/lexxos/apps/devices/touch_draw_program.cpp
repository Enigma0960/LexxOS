#include <lexxos/apps/devices/touch_draw_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/apps/devices/detail/touch_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::device_programs {

using detail::touch::defaultDrawRadius;
using detail::touch::defaultDrawSeconds;
using detail::touch::drawDot;
using detail::touch::drawLine;
using detail::touch::DrawTrack;
using detail::touch::drawTrackCount;
using detail::touch::recordDisplayActivityForTouch;
using detail::touch::selectTouch;
using detail::touch::touchDrawDelayMs;

#if LEXXOS_COMMAND_TOUCH_DRAW

namespace {

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

} // namespace

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
