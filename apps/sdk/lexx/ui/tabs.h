#pragma once

#include <lexx/ui/button.h>
#include <lexx/ui/tree_window_v3.h>

namespace lexx::ui {

// A tab page owns its widgets; mount/unmount callbacks manage only their
// presence in TreeWindowV3. Inactive pages consume no kernel control slots.
template <uint16_t WindowCapacity, uint16_t MaxTabs>
class Tabs {
public:
	using PageHandler = bool (*)(void*, bool visible);

	Tabs(TreeWindowV3<WindowCapacity>& window, LayoutGroup const& heading)
	    : _window(window)
	    , _heading(heading) {}

	Tabs(Tabs const&) = delete;
	Tabs& operator=(Tabs const&) = delete;

	bool add(Button& button, PageHandler page, void* context) {
		if (_count >= MaxTabs || page == nullptr) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].button == &button || _entries[i].button->id() == button.id()) {
				return false;
			}
		}

		_entries[_count] = {this, &button, page, context, _count};
		button.on_click(&Tabs::clicked, &_entries[_count]);
		if (!_window.add(button, &_heading)) {
			return false;
		}

		++_count;
		return true;
	}

	// Call once before show() to register the first page. Later switches use
	// the transactional kernel tree mutation API, without restarting the window.
	bool select(uint16_t index) {
		if (index >= _count || _selected == static_cast<int>(index)) {
			return index < _count;
		}

		int const previous = _selected;
		bool const live = _window.opened();
		if (live && _window.beginBatch() != 0) {
			return false;
		}

		if (previous >= 0 && !_entries[previous].page(_entries[previous].context, false)) {
			if (live) {
				_window.rollbackBatch();
			}

			return false;
		}

		if (!_entries[index].page(_entries[index].context, true)) {
			if (live) {
				_window.rollbackBatch();
			} else if (previous >= 0) {
				(void) _entries[previous].page(_entries[previous].context, true);
			}

			return false;
		}

		// In live mode, structural edits and all tab markers share one
		// system patch and consequently one display presentation.
		for (uint16_t i = 0; i < _count; ++i) {
			_entries[i].button->set_tab_active(i == index);
		}

		if (live && _window.commitBatch() != 0) {
			for (uint16_t i = 0; i < _count; ++i) {
				_entries[i].button->set_tab_active(static_cast<int>(i) == previous);
			}

			return false;
		}

		_selected = index;
		return true;
	}

	[[nodiscard]] int selected() const { return _selected; }

private:
	struct Entry {
		Tabs* tabs{nullptr};
		Button* button{nullptr};
		PageHandler page{nullptr};
		void* context{nullptr};
		uint16_t index{0};
	};

	static void clicked(void* context) {
		auto& entry = *static_cast<Entry*>(context);
		(void) entry.tabs->select(entry.index);
	}

	TreeWindowV3<WindowCapacity>& _window;
	LayoutGroup const& _heading;
	Entry _entries[MaxTabs]{};
	uint16_t _count{0};
	int _selected{-1};
};

} // namespace lexx::ui
