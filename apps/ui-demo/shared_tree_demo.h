#pragma once

#include <lexx/ui.h>

namespace lexxos::ui_demo {

// One shared gallery for RV32I and the native transport. Pages are mounted
// on demand, so even the Pico profile does not hold all widgets at once.
class TreeDemo final {
public:
	explicit TreeDemo(lexx::ui::Transport const& transport)
	    : _window("LexxUI Gallery", transport)
	    , _tabs(_window, _bar) {
		(void) _checks.add(_checkA);
		(void) _checks.add(_checkB);
		(void) _radios.add(_radioA);
		(void) _radios.add(_radioB);
		(void) _radios.select(0U);
	}

	int run() {
		char const* const* options = _options; // Static storage, never a stack array.
		if (_scroll.set_items(options, 15U) != 0 || _dropdown.set_items(options, 15U) != 0 || _secondaryDropdown.set_items(options, 15U) != 0) {
			return 1;
		}

		// The paged controls keep non-owning pointers: these string literals
		// remain valid throughout the lifetime of the demo.
		if (!_window.add(_root) || !_window.add(_bar, &_root) || !_window.add(_body, &_root) || !_tabs.add(_tabScroll, &TreeDemo::pageScroll, this) || !_tabs.add(_tabDropdown, &TreeDemo::pageDropdown, this) || !_tabs.add(_tabChecks, &TreeDemo::pageChecks, this) || !_tabs.add(_tabRadio, &TreeDemo::pageRadio, this) || !_tabs.select(0U)) {
			return 1;
		}

		return _window.exec();
	}

private:
	static inline char const* const _options[15]{
	    "Storage", "Display", "Keyboard", "Audio", "Network",
	    "Power", "Clock", "Services", "Terminal", "Files",
	    "Memory", "Security", "Diagnostics", "Info", "About"};

	static bool pageScroll(void* context, bool visible) {
		auto& self = *static_cast<TreeDemo*>(context);
		if (!visible) {
			return self._window.remove(self._scrollPage.id()) == 0;
		}

		return self._window.add(self._scrollPage, &self._body)
		    && self._window.add(self._scroll, &self._scrollPage)
		    && self._window.add(self._secondaryDropdown, &self._scrollPage);
	}

	static bool pageDropdown(void* context, bool visible) {
		auto& self = *static_cast<TreeDemo*>(context);
		if (!visible) {
			return self._window.remove(self._dropdownPage.id()) == 0;
		}

		return self._window.add(self._dropdownPage, &self._body) && self._window.add(self._dropdown, &self._dropdownPage);
	}

	static bool pageChecks(void* context, bool visible) {
		auto& self = *static_cast<TreeDemo*>(context);
		return visible ? self._checks.mount(self._window, self._body)
		               : self._checks.unmount(self._window) == 0;
	}

	static bool pageRadio(void* context, bool visible) {
		auto& self = *static_cast<TreeDemo*>(context);
		return visible ? self._radios.mount(self._window, self._body)
		               : self._radios.unmount(self._window) == 0;
	}

	lexx::ui::TreeWindowV3<16> _window;
	lexx::ui::Column _root{100};
	lexx::ui::RowContainer _bar{101};
	lexx::ui::Column _body{102};
	lexx::ui::Column _scrollPage{103};
	lexx::ui::Column _dropdownPage{104};
	lexx::ui::Button _tabScroll{1, "List"};
	lexx::ui::Button _tabDropdown{2, "Drop"};
	lexx::ui::Button _tabChecks{3, "Checks"};
	lexx::ui::Button _tabRadio{4, "Radio"};
	lexx::ui::ScrollList _scroll{10};
	lexx::ui::Dropdown _dropdown{11};
	lexx::ui::Dropdown _secondaryDropdown{12};
	lexx::ui::Checkbox _checkA{20, "WiFi", true};
	lexx::ui::Checkbox _checkB{21, "Audio", false};
	lexx::ui::RadioButton _radioA{30, "Mode A", true};
	lexx::ui::RadioButton _radioB{31, "Mode B"};
	lexx::ui::CheckboxGroup<2> _checks{105};
	lexx::ui::RadioGroup<2> _radios{106};
	lexx::ui::Tabs<16, 4> _tabs;
};

} // namespace lexxos::ui_demo
