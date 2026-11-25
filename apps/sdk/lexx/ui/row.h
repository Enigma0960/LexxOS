#pragma once

#include <lexx/ui/widget.h>

namespace lexx::ui {

// A non-owning horizontal group. Window registers each widget in document order.
// The system resolves all coordinates; no guest-side pixel calculations.
class Row {
public:
	bool add(Widget& widget) {
		if (_count == max_controls || widget.id() == 0U) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_items[i] == &widget || _items[i]->id() == widget.id()) {
				return false;
			}
		}

		_items[_count++] = &widget;
		return true;
	}

	[[nodiscard]] uint16_t size() const { return _count; }

	[[nodiscard]] Widget* at(uint16_t index) const { return index < _count ? _items[index] : nullptr; }

private:
	Widget* _items[max_controls]{};
	uint16_t _count{0};
};

} // namespace lexx::ui
