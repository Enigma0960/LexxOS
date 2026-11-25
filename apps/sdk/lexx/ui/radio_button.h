#pragma once

#include <lexx/ui/widget.h>

namespace lexx::ui {

class RadioButton final : public Widget {
public:
	using ChangeHandler = void (*)(void*, bool);

	RadioButton(uint16_t id, char const* text, bool checked = false)
	    : Widget(Kind::RadioButton, id, text, checked ? 1U : 0U) {
		setEventHandler(&RadioButton::handle, this);
	}

	[[nodiscard]] bool checked() const { return _control.value != 0U; }

	int set_checked(bool value, bool commit = true) {
		setLocalValue(value ? 1U : 0U);
		return commit ? synchronize() : 0;
	}

	void on_change(ChangeHandler callback, void* context = nullptr) {
		_handler = callback;
		_context = context;
	}

private:
	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<RadioButton*>(opaque);
		if (event.type == static_cast<uint16_t>(EventType::Change)) {
			self.setLocalValue(event.value != 0U ? 1U : 0U);
			if (self._handler != nullptr) {
				self._handler(self._context, self.checked());
			}
		}
	}

	ChangeHandler _handler{nullptr};
	void* _context{nullptr};
};

} // namespace lexx::ui
