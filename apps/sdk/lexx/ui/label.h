#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class Label final : public Widget {
public:
	Label(uint16_t id, char const* text)
	    : Widget(Kind::Label, id, text) {}

	int set_text(char const* text) {
		setLocalText(text);
		return synchronize();
	}
};

} // namespace lexx::ui
