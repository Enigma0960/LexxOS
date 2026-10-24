#include <lexxos/apps/utility/bootsel_program.h>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <pico/bootrom.h>
#endif

namespace lexxos::apps::utility_programs {

lexxos::program::ProgramDescriptor BootselProgram::descriptor() {
	return {
	    .name = "bootsel",
	    .help = "reboot RP2040 into USB BOOTSEL loader",
	    .usage = "bootsel",
	    .handler = run,
	};
}

int BootselProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
#if defined(LEXXOS_PLATFORM_PICO)
	context.writeLine("entering USB BOOTSEL loader");

	auto const& clock = context.platform().clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(50);
	}

	reset_usb_boot(0, 0);
	return 0;
#else
	context.writeLine("bootsel unavailable on this platform");
	return 1;
#endif
}

} // namespace lexxos::apps::utility_programs
