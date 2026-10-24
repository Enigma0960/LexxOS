#include <lexxos/apps/devices/detail/input_program_support.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/device_program_support.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs::detail {
namespace {

	constexpr lexxos::devices::KeyCode observedKeys[] = {
	    lexxos::devices::KeyCode::Up,
	    lexxos::devices::KeyCode::Down,
	    lexxos::devices::KeyCode::Left,
	    lexxos::devices::KeyCode::Right,
	    lexxos::devices::KeyCode::Enter,
	    lexxos::devices::KeyCode::Escape,
	    lexxos::devices::KeyCode::Backspace,
	    lexxos::devices::KeyCode::Space,
	    lexxos::devices::KeyCode::A,
	    lexxos::devices::KeyCode::B,
	    lexxos::devices::KeyCode::C,
	    lexxos::devices::KeyCode::D,
	    lexxos::devices::KeyCode::X,
	    lexxos::devices::KeyCode::Y,
	    lexxos::devices::KeyCode::Z,
	    lexxos::devices::KeyCode::Num0,
	    lexxos::devices::KeyCode::Num1,
	    lexxos::devices::KeyCode::Num2,
	    lexxos::devices::KeyCode::Num3,
	    lexxos::devices::KeyCode::Num4,
	    lexxos::devices::KeyCode::Num5,
	    lexxos::devices::KeyCode::Num6,
	    lexxos::devices::KeyCode::Num7,
	    lexxos::devices::KeyCode::Num8,
	    lexxos::devices::KeyCode::Num9,
	    lexxos::devices::KeyCode::Soft1,
	    lexxos::devices::KeyCode::Soft2,
	    lexxos::devices::KeyCode::Star,
	    lexxos::devices::KeyCode::Sharp,
	    lexxos::devices::KeyCode::Power,
	    lexxos::devices::KeyCode::Reserved,
	};
	constexpr std::uint32_t interactiveInputLockRetryMs = 200;
	constexpr std::uint32_t interactiveInputLockRetrySleepMs = 2;

	[[nodiscard]] std::uint32_t keyMask(lexxos::devices::KeyCode key) {
		auto const index = static_cast<std::uint32_t>(key);
		return index < 32 ? (1U << index) : 0U;
	}

	void writeEscapedCharacter(lexxos::os::AppContext& context, std::uint8_t character) {
		switch (character) {
			case 0:
				return;
			case '\n':
				context.write("\\n");
				return;
			case '\r':
				context.write("\\r");
				return;
			case '\t':
				context.write("\\t");
				return;
			case '\b':
				context.write("\\b");
				return;
			case '\\':
				context.write("\\\\\\\\");
				return;
			case '\'':
				context.write("\\'");
				return;
			default:
				break;
		}

		if (character >= 32 && character <= 126) {
			char text[2]{static_cast<char>(character), '\0'};
			context.write(text);
			return;
		}

		context.write("?");
	}

	void writeModifierList(lexxos::os::AppContext& context, std::uint8_t modifiers) {
		bool any = false;
		auto writeModifier = [&](std::uint8_t mask, std::string_view name) {
			if ((modifiers & mask) == 0) {
				return;
			}
			context.write(any ? "," : "");
			context.write(name);
			any = true;
		};

		writeModifier(lexxos::devices::InputModifierAlt, "alt");
		writeModifier(lexxos::devices::InputModifierShiftLeft, "shift-left");
		writeModifier(lexxos::devices::InputModifierShiftRight, "shift-right");
		writeModifier(lexxos::devices::InputModifierSym, "sym");
		writeModifier(lexxos::devices::InputModifierCtrl, "ctrl");
	}

} // namespace

void writeBoolField(lexxos::os::AppContext& context, std::string_view name, bool value) {
	context.write(name);
	context.write(":");
	context.write(value ? "1" : "0");
	context.write(" ");
}

std::uint32_t pressedKeyMask(lexxos::devices::InputDevice& input) {
	std::uint32_t mask = 0;
	for (auto const key : observedKeys) {
		if (input.isPressed(key)) {
			mask |= keyMask(key);
		}
	}
	return mask;
}

lexxos::devices::DeviceResourceLock lockInputDeviceForInteractiveProgram(
    lexxos::os::AppContext& context,
    lexxos::devices::InputDevice& input,
    std::string_view owner) {
	auto const startedMs = uptimeMs(context);
	while (true) {
		auto lock = context.devices().lockInputDevice(input.name(), lexxos::devices::DeviceResourcePriority::Interactive, owner);
		if (lock) {
			return lock;
		}
		if (uptimeMs(context) - startedMs >= interactiveInputLockRetryMs) {
			return {};
		}
		lexxos::kernel::sleepMs(interactiveInputLockRetrySleepMs);
	}
}

void writePressedKeys(lexxos::os::AppContext& context, std::uint32_t mask) {
	context.write("pressed:");
	bool any = false;
	for (auto const key : observedKeys) {
		auto const bit = keyMask(key);
		if (bit != 0 && (mask & bit) != 0) {
			context.write(any ? "," : " ");
			context.write(lexxos::devices::toString(key));
			any = true;
		}
	}
	if (!any) {
		context.write(" <none>");
	}
	context.write(" mask=");
	writeHex32(context, mask);
	context.writeLine("");
}

void writeRawKeyboardState(lexxos::os::AppContext& context, lexxos::devices::InputDevice& input) {
	auto const raw = input.rawState();
	auto const rows = input.rawRowCount();
	auto const columns = input.rawColumnCount();
	auto const keyCount = input.rawKeyCount();

	context.write("raw: mask=");
	writeHex32(context, raw);
	context.write(" keys=");
	writeUnsigned(context, keyCount);
	if (rows != 0 && columns != 0) {
		context.write(" matrix=");
		writeUnsigned(context, rows);
		context.write("x");
		writeUnsigned(context, columns);
	}

	bool any = false;
	for (std::uint32_t index = 0; index < keyCount && index < 32; ++index) {
		auto const bit = 1U << index;
		if ((raw & bit) == 0) {
			continue;
		}

		context.write(any ? "," : " active=");
		context.write("[");
		writeUnsigned(context, index);
		if (columns != 0) {
			context.write(" r");
			writeUnsigned(context, index / columns);
			context.write("c");
			writeUnsigned(context, index % columns);
		}
		context.write("]");
		any = true;
	}

	if (!any) {
		context.write(" active=<none>");
	}
	context.writeLine("");
}

void writeInputEventDetails(lexxos::os::AppContext& context, lexxos::devices::InputEvent const& event) {
	if (event.character != 0) {
		context.write(" char='");
		writeEscapedCharacter(context, event.character);
		context.write("'");
	}

	if (event.scanCode != 0) {
		context.write(" scan=");
		writeHex8(context, event.scanCode);
	}

	if (event.modifiers != 0) {
		context.write(" mods=");
		writeModifierList(context, event.modifiers);
		context.write("(");
		writeHex8(context, event.modifiers);
		context.write(")");
	}
}

std::uint32_t drainInputEvents(lexxos::os::AppContext& context, lexxos::devices::InputDevice& input) {
	lexxos::devices::InputEvent event;
	std::uint32_t count = 0;
	for (std::uint32_t index = 0; index < 16; ++index) {
		if (!input.pollEvent(event)) {
			break;
		}

		context.write("event: ");
		context.write(lexxos::devices::toString(event.type));
		context.write(" ");
		context.write(lexxos::devices::toString(event.key));
		writeInputEventDetails(context, event);
		context.write(" at ");
		writeUnsigned(context, event.timestampMs);
		context.writeLine(" ms");
		++count;
	}
	return count;
}

} // namespace lexxos::apps::device_programs::detail
