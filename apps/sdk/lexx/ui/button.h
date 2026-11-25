#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class Button final : public Widget {
public:
	using ClickHandler = void (*)(void*);

	Button(uint16_t id, char const* title)
	    : Widget(Kind::Button, id, title) {
		setEventHandler(&Button::handle, this);
	}

	void on_click(ClickHandler handler, void* context = nullptr) {
		_handler = handler;
		_context = context;
	}

	int set_text(char const* text) {
		setLocalText(text);
		return synchronize();
	}

	// Active tab is a persistent visual state, not keyboard focus.
	// Tabs batches all markers with TreeWindowV3::update().
	void set_tab_active(bool active) {
		_control.flags = static_cast<uint8_t>(active
		        ? (_control.flags | 8U)
		        : (_control.flags & ~8U));
	}

private:
	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<Button*>(opaque);
		if (event.type == static_cast<uint16_t>(EventType::Activate) && self._handler != nullptr) {
			self._handler(self._context);
		}
	}

	ClickHandler _handler{nullptr};
	void* _context{nullptr};
};

} // namespace lexx::ui
