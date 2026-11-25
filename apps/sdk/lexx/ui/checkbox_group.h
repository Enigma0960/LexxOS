#pragma once

#include <lexx/ui/checkbox.h>
#include <lexx/ui/radio_button.h>
#include <lexx/ui/tree_window_v3.h>

namespace lexx::ui {

template <uint16_t Capacity>
class CheckboxGroup {
public:
	static_assert(Capacity > 0 && Capacity <= 32U);

	explicit CheckboxGroup(uint16_t groupId)
	    : _group(groupId) {}

	bool add(Checkbox& checkbox) {
		if (_count == Capacity || checkbox.attached()) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_items[i] == &checkbox || _items[i]->id() == checkbox.id()) {
				return false;
			}
		}

		_items[_count++] = &checkbox;
		return true;
	}

	template <uint16_t WindowCapacity>
	bool mount(TreeWindowV3<WindowCapacity>& window, LayoutGroup const& parent) {
		if (!window.add(_group, &parent)) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (!window.add(*_items[i], &_group)) {
				(void) window.remove(_group.id());
				return false;
			}
		}

		return true;
	}

	template <uint16_t WindowCapacity>
	int unmount(TreeWindowV3<WindowCapacity>& window) {
		return window.remove(_group.id());
	}

	[[nodiscard]] uint32_t selected_mask() const {
		uint32_t result = 0U;
		for (uint16_t i = 0; i < _count; ++i) {
			if (_items[i]->checked()) {
				result |= 1UL << i;
			}
		}

		return result;
	}

private:
	Column _group;
	Checkbox* _items[Capacity]{};
	uint16_t _count{0};
};

} // namespace lexx::ui
