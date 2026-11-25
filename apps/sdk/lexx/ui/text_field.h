#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class TextField final : public Widget {
public:
	using ChangeHandler = void (*)(void*, char const*);

	TextField(uint16_t id, char const* value = "")
	    : Widget(Kind::TextField, id, value) {
		setEventHandler(&TextField::handle, this);
	}

	[[nodiscard]] char const* text() const { return _control.text; }

	int set_text(char const* value) {
		setLocalText(value);
		return synchronize();
	}

	void on_change(ChangeHandler handler, void* context = nullptr) {
		_handler = handler;
		_context = context;
	}

private:
	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<TextField*>(opaque);
		if (event.type == static_cast<uint16_t>(EventType::Change)) {
			self.setLocalText(event.text);
			if (self._handler != nullptr) {
				self._handler(self._context, self.text());
			}
		}
	}

	ChangeHandler _handler{nullptr};
	void* _context{nullptr};
};

} // namespace lexx::ui
