#pragma once

#include <lexx/ui/transport.h>

namespace lexx::ui {

enum class MessageKind : uint8_t {
	Information = 1,
	Warning = 2,
	Error = 3,
	Question = 4,
};

enum class MessageButtons : uint8_t {
	Ok = 1,
	OkCancel = 2,
};

enum class DialogResult : int {
	Failed = -1,
	Ok = 1,
	Cancel = 2,
};

class MessageBox {
public:
	MessageBox(uint16_t id, MessageKind kind, MessageButtons buttons, char const* title, char const* message) {
		_dialog.id = id;
		_dialog.kind = static_cast<uint8_t>(kind);
		_dialog.buttons = static_cast<uint8_t>(buttons);
		copy(_dialog.title, title);
		copy(_dialog.message, message);
	}

	static MessageBox information(char const* title, char const* message, uint16_t id = 1) {
		return {id, MessageKind::Information, MessageButtons::Ok, title, message};
	}

	static MessageBox warning(char const* title, char const* message, uint16_t id = 1) {
		return {id, MessageKind::Warning, MessageButtons::Ok, title, message};
	}

	static MessageBox error(char const* title, char const* message, uint16_t id = 1) {
		return {id, MessageKind::Error, MessageButtons::Ok, title, message};
	}

	static MessageBox question(char const* title, char const* message, uint16_t id = 1) {
		return {id, MessageKind::Question, MessageButtons::OkCancel, title, message};
	}

	[[nodiscard]] int show(Transport const& transport) const { return transport.show_dialog(_dialog); }

	// The dialog is modal in the OS; this method waits cooperatively.
	[[nodiscard]] DialogResult exec(Transport const& transport, uint32_t pollMs = 20U) const {
		if (show(transport) != 0) {
			return DialogResult::Failed;
		}

		for (;;) {
			Event event{};
			int const status = transport.poll(event);
			if (status < 0) {
				return DialogResult::Failed;
			}

			if (status > 0 && event.type == static_cast<uint16_t>(EventType::DialogResult) && event.id == _dialog.id) {
				return static_cast<DialogResult>(event.value);
			}

			transport.sleep_ms(pollMs);
		}
	}

#if !defined(LEXXOS_UI_NATIVE)
	[[nodiscard]] int show() const { return show(guestTransport); }

	[[nodiscard]] DialogResult exec(uint32_t pollMs = 20U) const { return exec(guestTransport, pollMs); }
#endif

private:
	template <size_t N>
	static void copy(char (&buffer)[N], char const* source) {
		uint32_t i = 0;
		if (source != nullptr) {
			while (i + 1 < N && source[i] != '\0') {
				buffer[i] = source[i];
				++i;
			}

			if (source[i] != '\0') {
				while (i != 0U && (static_cast<uint8_t>(source[i]) & 0xC0U) == 0x80U) {
					--i;
				}
			}
		}

		buffer[i] = '\0';
	}

	DialogDescriptor _dialog{};
};

} // namespace lexx::ui
