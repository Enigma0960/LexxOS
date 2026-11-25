#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class Checkbox final : public Widget {
public:
	using ChangeHandler = void (*)(void*, bool);

	Checkbox(uint16_t id, char const* title, bool checked = false)
	    : Widget(Kind::Checkbox, id, title, checked ? 1U : 0U) {
		setEventHandler(&Checkbox::handle, this);
	}

	[[nodiscard]] bool checked() const { return _control.value != 0U; }

	int set_checked(bool checked) {
		setLocalValue(checked ? 1U : 0U);
		return synchronize();
	}

	void on_change(ChangeHandler handler, void* context = nullptr) {
		_handler = handler;
		_context = context;
	}

private:
	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<Checkbox*>(opaque);
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
