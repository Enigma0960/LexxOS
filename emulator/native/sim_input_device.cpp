#include <sim_input_device.h>

#include <sim_runtime.h>

using lexxos::emulator::SimInputDevice;
using lexxos::emulator::SimInputEvent;

std::string_view SimInputDevice::name() const {
	return "sim-keyboard";
}

bool SimInputDevice::isReady() const {
	return lexxos::emulator::activeRuntime() != nullptr;
}

bool SimInputDevice::pollEvent(lexxos::devices::InputEvent& event) {
	auto* runtime = lexxos::emulator::activeRuntime();
	if (runtime == nullptr) {
		return false;
	}

	SimInputEvent simEvent{};
	if (!runtime->inputPollEvent(simEvent)) {
		return false;
	}

	event = {
	    .type = simEvent.pressed ? lexxos::devices::InputEventType::KeyDown : lexxos::devices::InputEventType::KeyUp,
	    .key = static_cast<lexxos::devices::KeyCode>(simEvent.key),
	    .timestampMs = simEvent.timestampMs,
	    .character = simEvent.character,
	    .modifiers = simEvent.modifiers,
	};
	return true;
}

bool SimInputDevice::isPressed(lexxos::devices::KeyCode key) const {
	auto* runtime = lexxos::emulator::activeRuntime();
	if (runtime == nullptr) {
		return false;
	}

	return runtime->inputIsPressed(static_cast<std::uint16_t>(key));
}

SimInputDevice& lexxos::emulator::simInputDevice() {
	static SimInputDevice device;
	return device;
}
