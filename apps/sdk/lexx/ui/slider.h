#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class Slider final : public Widget {
public:
	using ChangeHandler = void (*)(void*, uint32_t);

	Slider(uint16_t id, uint32_t percent = 0)
	    : Widget(Kind::Slider, id, "", clamp(percent)) {
		setEventHandler(&Slider::handle, this);
	}

	[[nodiscard]] uint32_t value() const { return _control.value; }

	int set_value(uint32_t percent) {
		setLocalValue(clamp(percent));
		return synchronize();
	}

	void on_change(ChangeHandler handler, void* context = nullptr) {
		_handler = handler;
		_context = context;
	}

private:
	static constexpr uint32_t clamp(uint32_t value) { return value > 100U ? 100U : value; }

	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<Slider*>(opaque);
		if (event.type == static_cast<uint16_t>(EventType::Change)) {
			self.setLocalValue(clamp(event.value));
			if (self._handler != nullptr) {
				self._handler(self._context, self.value());
			}
		}
	}

	ChangeHandler _handler{nullptr};
	void* _context{nullptr};
};

} // namespace lexx::ui
