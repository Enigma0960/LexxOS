#include <cstdlib>
#include <iostream>

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/platform/platform_services.h>

namespace {

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

unsigned initialized{}, sleeps{}, writes{};
bool pinValue{};
lexxos::platform::LegacyBoardDescriptor boardDescriptor{};

lexxos::platform::LegacyBoardDescriptor const& currentBoard() {
	return boardDescriptor;
}

void initialize() {
	++initialized;
}

void sleep(std::uint32_t ms) {
	sleeps += ms;
}

bool configureOutput(lexxos::platform::GpioPin pin, bool value) {
	expect(pin.number == 7, "unexpected output pin");
	pinValue = value;
	return true;
}

bool writePin(lexxos::platform::GpioPin pin, bool value) {
	expect(pin.number == 7, "unexpected write pin");
	pinValue = value;
	++writes;
	return true;
}

void registerDevices(lexxos::devices::DeviceManager& devices, lexxos::platform::PlatformServices const& platform) {
	expect(initialized == 1 && devices.gpioPinCount() == 0, "bootstrap order incorrect");
	expect(platform.gpio.configureOutput({7}, false), "output initialization failed");
	expect(devices.registerGpioPin("output", "mock GPIO", {7}, false, true), "GPIO registration failed");
}

} // namespace

// Fallback-символы линкера: тест не допускает обращения к глобальной платформе вместо внедрённой.
namespace lexxos::platform {

LegacyBoardDescriptor const& board() {
	expect(false, "global board used");
	return boardDescriptor;
}

void sleepMs(std::uint32_t) {
	expect(false, "global clock used");
}

void writeConsole(std::string_view) {
	expect(false, "headless bootstrap wrote to console");
}

} // namespace lexxos::platform

int main() {
	namespace core = lexxos::core;
	namespace kernel = lexxos::kernel;
	namespace lifecycle = lexxos::lifecycle;
	using lexxos::runtime::Capability;
	boardDescriptor.userGpio = {.pins = {{{7}, "output", false, true}}, .count = 1};

	lexxos::platform::PlatformServices platform{
	    .lifecycle = {.initialize = initialize},
	    .board = {.current = currentBoard},
	    .clock = {.sleepMs = sleep},
	    .gpio = {.configureOutput = configureOutput, .write = writePin},
	};

	lifecycle::EventBus events;
	core::CoreSystem system{platform, events};
	auto context = system.createContext();
	kernel::requestShutdown();
	core::bootstrap(system, registerDevices);
	expect(!kernel::isShutdownRequested() && initialized == 1, "platform startup incomplete");
	expect(&context.devices() == &system.devices() && &context.platform() == &platform, "context copied shared state");
	expect(context.capabilities().has(Capability::GpioUser), "GPIO capability missing");
	expect(!context.capabilities().has(Capability::Display) && !context.capabilities().has(Capability::StorageBlock), "headless capabilities incorrect");
	auto const* output = context.devices().gpioPin("output");
	expect(output && context.platform().gpio.write(output->pin, true), "context cannot use GPIO");
	expect(pinValue && writes == 1, "GPIO service not called");
	kernel::schedulerBackend().sleepMs(7);
	kernel::schedulerBackend().yield();
	expect(sleeps == 7, "scheduler not bound to supplied clock");
	lifecycle::EventCursor cursor;
	events.reset(cursor);
	expect(context.events().publish({.type = lifecycle::EventType::ShutdownRequested}), "context event failed");
	lifecycle::Event event;
	expect(events.poll(cursor, event) && event.type == lifecycle::EventType::ShutdownRequested, "context bus not shared");
	// Уже созданный контекст должен видеть новые capabilities, а не их прежний снимок.
	boardDescriptor.userGpio.count = 0;
	system.refreshCapabilities();
	expect(!context.capabilities().has(Capability::GpioUser), "context capabilities stale");
	lexxos::platform::PlatformServices optionalPlatform{.board = {.current = currentBoard}};
	lifecycle::EventBus optionalEvents;
	core::CoreSystem optional{optionalPlatform, optionalEvents};
	core::bootstrap(optional);
	expect(optional.devices().gpioPinCount() == 0 && initialized == 1, "optional lifecycle required");
	expect(system.devices().gpioPinCount() == 1 && &optional.events() != &events, "independent core state leaked");
	system.initialize();
	expect(system.devices().gpioPinCount() == 0 && initialized == 1, "registry reset reinitialized hardware");
}
