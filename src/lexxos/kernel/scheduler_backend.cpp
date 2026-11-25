#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

namespace {

lexxos::platform::PlatformServices const* g_platformServices{nullptr};

void fallbackSleepMs(std::uint32_t milliseconds) {
#if defined(LEXXOS_EDITION_CE)
	(void) milliseconds;
#else
	lexxos::platform::sleepMs(milliseconds);
#endif
}

void fallbackYield() {
	fallbackSleepMs(0);
}

void fallbackWriteConsole(std::string_view text) {
#if defined(LEXXOS_EDITION_CE)
	(void) text;
#else
	lexxos::platform::writeConsole(text);
#endif
}

void serviceSleepMs(std::uint32_t milliseconds) {
	auto const* platform = g_platformServices;
	if (platform != nullptr && platform->clock.sleepMs != nullptr) {
		platform->clock.sleepMs(milliseconds);
		return;
	}

	fallbackSleepMs(milliseconds);
}

void serviceYield() {
	serviceSleepMs(0);
}

void serviceWriteConsole(std::string_view text) {
	auto const* platform = g_platformServices;
	if (platform != nullptr && platform->console.write != nullptr) {
		platform->console.write(text);
		return;
	}

	fallbackWriteConsole(text);
}

constexpr lexxos::kernel::SchedulerBackend kFallbackBackend{
    .sleepMs = fallbackSleepMs,
    .yield = fallbackYield,
    .writeConsole = fallbackWriteConsole,
};

lexxos::kernel::SchedulerBackend normalize(lexxos::kernel::SchedulerBackend backend) {
	if (backend.sleepMs == nullptr) {
		backend.sleepMs = kFallbackBackend.sleepMs;
	}

	if (backend.yield == nullptr) {
		backend.yield = kFallbackBackend.yield;
	}

	if (backend.writeConsole == nullptr) {
		backend.writeConsole = kFallbackBackend.writeConsole;
	}

	return backend;
}

lexxos::kernel::SchedulerBackend g_schedulerBackend = kFallbackBackend;

} // namespace

void lexxos::kernel::bindSchedulerBackend(SchedulerBackend backend) {
	g_platformServices = nullptr;
	g_schedulerBackend = normalize(backend);
}

void lexxos::kernel::bindSchedulerBackend(lexxos::platform::PlatformServices const& platform) {
	g_platformServices = &platform;
	g_schedulerBackend = SchedulerBackend{
	    .sleepMs = serviceSleepMs,
	    .yield = serviceYield,
	    .writeConsole = serviceWriteConsole,
	};
}

lexxos::kernel::SchedulerBackend const& lexxos::kernel::schedulerBackend() {
	return g_schedulerBackend;
}
