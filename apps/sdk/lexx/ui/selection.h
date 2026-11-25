#pragma once

#include <lexx/ui/widget.h>

namespace lexx::ui {

// Keeps caller-owned strings outside the fixed-size wire record. The host
// receives only the currently visible page (at most eight entries).
template <Kind ControlKind, uint16_t PageSize>
class PagedSelection : public Widget {
public:
	static_assert(PageSize > 0U && PageSize <= max_list_items);

	using SelectionHandler = void (*)(void*, uint16_t, char const*);

	explicit PagedSelection(uint16_t id)
	    : Widget(ControlKind, id) {
		_items.id = id;
		setEventHandler(&PagedSelection::handle, this);
		setAttachHandler(&PagedSelection::attach, this);
	}

	int set_items(char const* const* entries, uint16_t count) {
		if ((entries == nullptr && count != 0U) || count > 256U) {
			return -1;
		}

		_entries = entries;
		_total = count;
		_first = 0U;
		setLocalValue(0U);
		return refresh();
	}

	[[nodiscard]] uint16_t selected_index() const {
		return static_cast<uint16_t>(_first + (_control.value & 0x7FFFFFFFU));
	}

	void on_change(SelectionHandler handler, void* context = nullptr) {
		_change = handler;
		_changeContext = context;
	}

	void on_activate(SelectionHandler handler, void* context = nullptr) {
		_activate = handler;
		_activateContext = context;
	}

private:
	int refresh() {
		ListItems pending{};
		pending.id = id();
		pending.count = static_cast<uint16_t>(
		    _total - _first < PageSize ? _total - _first : PageSize);
		for (uint16_t i = 0; i < pending.count; ++i) {
			copyText(pending.entries[i], _entries[_first + i]);
		}

		_items = pending;
		if (!attached() || transport() == nullptr) {
			return 0;
		}

		int const result = transport()->set_items(_items);
		return result == 0 ? synchronize() : result;
	}

	static int attach(void* opaque) {
		auto& self = *static_cast<PagedSelection*>(opaque);
		return self.transport() != nullptr ? self.transport()->set_items(self._items) : -1;
	}

	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<PagedSelection*>(opaque);
		if (event.value == 0xFFFFFFFEU || event.value == 0xFFFFFFFFU) {
			bool const back = event.value == 0xFFFFFFFEU;
			if (back ? self._first > 0U : static_cast<uint32_t>(self._first) + PageSize < self._total) {
				self._first = static_cast<uint16_t>(back ? self._first - 1U : self._first + 1U);
				uint32_t const expanded = self._control.value & 0x80000000U;
				uint32_t const last = self._total - self._first < PageSize
				    ? self._total - self._first
				    : PageSize;
				self.setLocalValue(expanded | (back ? 0U : last - 1U));
				(void) self.refresh();
				self.notify(self._change, self._changeContext);
			}

			return;
		}

		if (event.type == static_cast<uint16_t>(EventType::Change)) {
			uint16_t const previous = self.selected_index();
			self.setLocalValue(event.value);
			if (self.selected_index() != previous) {
				self.notify(self._change, self._changeContext);
			}
		} else if (event.type == static_cast<uint16_t>(EventType::Activate)) {
			self.notify(self._activate, self._activateContext);
		}
	}

	void notify(SelectionHandler handler, void* context) const {
		uint16_t const index = selected_index();
		if (handler != nullptr && index < _total) {
			handler(context, index, _entries[index]);
		}
	}

	ListItems _items{};
	char const* const* _entries{nullptr};
	uint16_t _total{0};
	uint16_t _first{0};
	SelectionHandler _change{nullptr};
	void* _changeContext{nullptr};
	SelectionHandler _activate{nullptr};
	void* _activateContext{nullptr};
};

using ScrollList = PagedSelection<Kind::ScrollList, 4U>;
using Dropdown = PagedSelection<Kind::Dropdown, 2U>;

} // namespace lexx::ui
