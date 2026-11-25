#pragma once
#include <lexx/ui/widget.h>

namespace lexx::ui {

class ListView final : public Widget {
public:
	using SelectionHandler = void (*)(void*, uint32_t, char const*);

	explicit ListView(uint16_t id)
	    : Widget(Kind::ListView, id) {
		_items.id = id;
		setEventHandler(&ListView::handle, this);
		setAttachHandler(&ListView::attach, this);
	}

	// Bounded list protocol: one ListView and at most eight choices per window.
	int set_items(char const* const* entries, uint16_t count) {
		if (count > max_list_items || (entries == nullptr && count != 0U)) {
			return -1;
		}

		ListItems pending{};
		pending.id = id();
		pending.count = count;
		for (uint16_t i = 0; i < count; ++i) {
			copyText(pending.entries[i], entries[i]);
		}

		_items = pending;
		return attached() && transport() != nullptr ? transport()->set_items(_items) : 0;
	}

	[[nodiscard]] uint32_t selected_index() const { return _control.value; }

	void on_change(SelectionHandler handler, void* context = nullptr) {
		_change = handler;
		_changeContext = context;
	}

	void on_activate(SelectionHandler handler, void* context = nullptr) {
		_activate = handler;
		_activateContext = context;
	}

private:
	static int attach(void* opaque) {
		auto& self = *static_cast<ListView*>(opaque);
		return self.transport() != nullptr ? self.transport()->set_items(self._items) : -1;
	}

	static void handle(void* opaque, Event const& event) {
		auto& self = *static_cast<ListView*>(opaque);
		if (event.type == static_cast<uint16_t>(EventType::Change)) {
			self.setLocalValue(event.value);
			if (self._change != nullptr) {
				self._change(self._changeContext, event.value, event.text);
			}
		} else if (event.type == static_cast<uint16_t>(EventType::Activate) && self._activate != nullptr) {
			self._activate(self._activateContext, event.value, event.text);
		}
	}

	ListItems _items{};
	SelectionHandler _change{nullptr};
	void* _changeContext{nullptr};
	SelectionHandler _activate{nullptr};
	void* _activateContext{nullptr};
};

} // namespace lexx::ui
