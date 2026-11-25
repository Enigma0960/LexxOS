#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class ProgressBar final : public Widget {
public:
	ProgressBar(uint16_t id, uint32_t percent = 0)
	    : Widget(Kind::ProgressBar, id, "", clamp(percent)) {}

	[[nodiscard]] uint32_t value() const { return _control.value; }

	int set_value(uint32_t percent) {
		setLocalValue(clamp(percent));
		return synchronize();
	}

private:
	static constexpr uint32_t clamp(uint32_t value) { return value > 100U ? 100U : value; }
};

} // namespace lexx::ui
