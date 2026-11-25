#pragma once

#include <lexx/ui/checkbox.h>
#include <lexx/ui/radio_button.h>
#include <lexx/ui/tree_window_v3.h>

namespace lexx::ui {

template <uint16_t Capacity>
class RadioGroup {
public:
	static_assert(Capacity > 0 && Capacity <= 16U);

	explicit RadioGroup(uint16_t groupId)
	    : _group(groupId) {}

	RadioGroup(RadioGroup const&) = delete;
	RadioGroup& operator=(RadioGroup const&) = delete;

	bool add(RadioButton& button) {
		if (_count == Capacity || button.attached()) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_items[i] == &button || _items[i]->id() == button.id()) {
				return false;
			}
		}

		if (button.checked() && selected_index() >= 0) {
			(void) button.set_checked(false, false);
		}

		_items[_count] = &button;
		_slots[_count] = {this, _count};
		button.on_change(&RadioGroup::onChanged, &_slots[_count]);
		++_count;
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

		_commitContext = &window;
		_commit = [](void* context, uint16_t id) {
			return static_cast<TreeWindowV3<WindowCapacity>*>(context)->update(id);
		};
		return true;
	}

	template <uint16_t WindowCapacity>
	int unmount(TreeWindowV3<WindowCapacity>& window) {
		int const result = window.remove(_group.id());
		if (result == 0) {
			_commit = nullptr;
			_commitContext = nullptr;
		}

		return result;
	}

	[[nodiscard]] int selected_index() const {
		for (uint16_t i = 0; i < _count; ++i) {
			if (_items[i]->checked()) {
				return i;
			}
		}

		return -1;
	}

	int select(uint16_t index) {
		if (index >= _count) {
			return -1;
		}

		bool previous[Capacity]{};
		for (uint16_t i = 0; i < _count; ++i) {
			previous[i] = _items[i]->checked();
			(void) _items[i]->set_checked(i == index, false);
		}

		int const result = _commit != nullptr ? _commit(_commitContext, _group.id()) : 0;
		if (result != 0) {
			for (uint16_t i = 0; i < _count; ++i) {
				(void) _items[i]->set_checked(previous[i], false);
			}
		}

		return result;
	}

private:
	struct Slot {
		RadioGroup* group{nullptr};
		uint16_t index{0};
	};

	static void onChanged(void* context, bool checked) {
		auto& slot = *static_cast<Slot*>(context);
		if (checked) {
			(void) slot.group->select(slot.index);
		}
	}

	Column _group;
	RadioButton* _items[Capacity]{};
	Slot _slots[Capacity]{};
	uint16_t _count{0};
	int (*_commit)(void*, uint16_t){nullptr};
	void* _commitContext{nullptr};
};

} // namespace lexx::ui
