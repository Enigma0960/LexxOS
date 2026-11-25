#pragma once

#include <lexx/ui/column.h>
#include <lexx/ui/row_container.h>
#include <lexx/ui/transport.h>
#include <lexx/ui/tree_document.h>
#include <lexx/ui/widget.h>

namespace lexx::ui {

// Non-owning layout declarations. Both native and guest apps submit the same
// tree; geometry, input routing and clipping remain system responsibilities.

// Distinct from the legacy, flat-ABI Row.

class TreeWindow final {
public:
	TreeWindow(char const* title, Transport const& transport)
	    : _transport(transport) { copyTitle(_title, title); }

#if !defined(LEXXOS_UI_NATIVE)
	explicit TreeWindow(char const* title)
	    : TreeWindow(title, guestTransport) {}
#endif

	TreeWindow(TreeWindow const&) = delete;
	TreeWindow& operator=(TreeWindow const&) = delete;

	~TreeWindow() {
		if (_open) {
			(void) close();
		}
	}

	bool add(LayoutGroup& group, LayoutGroup const* parent = nullptr) {
		return insert(nullptr, &group, parent);
	}

	bool add(Widget& widget, LayoutGroup const* parent = nullptr) {
		return insert(&widget, nullptr, parent);
	}

	int show() {
		if (_open) {
			return -1;
		}

		TreeDocument document{};
		copyTitle(document.title, _title);
		document.count = _count;
		for (uint16_t i = 0; i < _count; ++i) {
			TreeNode& node = document.nodes[i];
			node.parentId = _entries[i].parentId;
			if (_entries[i].group != nullptr) {
				node.control.id = _entries[i].group->id();
				node.control.kind = _entries[i].group->kind();
				node.control.flags = flag_auto_layout;
			} else {
				node.control = _entries[i].widget->descriptor();
				// Explicit pixel positions and legacy same-row flags are not
				// meaningful in a nested layout document.
				if ((node.control.flags & flag_auto_layout) == 0U || (node.control.flags & flag_same_row) != 0U) {
					return -1;
				}
			}
		}

		int const status = _transport.show_tree(document);
		if (status != 0) {
			return status;
		}

		_open = true;
		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr) {
				int const attached = _entries[i].widget->activate(_transport);
				if (attached != 0) {
					(void) close();
					return attached;
				}
			}
		}

		return 0;
	}

	int poll() {
		if (!_open) {
			return -1;
		}

		Event event{};
		int const result = _transport.poll(event);
		if (result <= 0) {
			return result;
		}

		if (event.type == static_cast<uint16_t>(EventType::Close)) {
			(void) close();
			return 1;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr && _entries[i].widget->id() == event.id) {
				_entries[i].widget->dispatch(event);
				break;
			}
		}

		return result;
	}

	int exec(uint32_t intervalMs = 20U) {
		if (!_open && show() != 0) {
			return -1;
		}

		while (_open) {
			if (poll() < 0) {
				(void) close();
				return -1;
			}

			if (_open) {
				_transport.sleep_ms(intervalMs);
			}
		}

		return 0;
	}

	int close() {
		if (!_open) {
			return 0;
		}

		_open = false;
		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr) {
				_entries[i].widget->deactivate();
			}
		}

		return _transport.close();
	}

	[[nodiscard]] bool is_open() const { return _open; }

private:
	struct Entry {
		Widget* widget{nullptr};
		LayoutGroup* group{nullptr};
		uint16_t parentId{0};
	};

	bool insert(Widget* widget, LayoutGroup* group, LayoutGroup const* parent) {
		uint16_t const id = widget != nullptr ? widget->id() : group->id();
		if (_open || id == 0U || _count >= max_tree_nodes) {
			return false;
		}

		if (parent != nullptr) {
			bool found = false;
			for (uint16_t i = 0; i < _count; ++i) {
				found |= _entries[i].group == parent;
			}

			if (!found) {
				return false;
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			uint16_t const other = _entries[i].widget != nullptr ? _entries[i].widget->id() : _entries[i].group->id();
			if (other == id || (widget != nullptr && _entries[i].widget == widget) || (group != nullptr && _entries[i].group == group)) {
				return false;
			}
		}

		_entries[_count++] = {widget, group, static_cast<uint16_t>(parent == nullptr ? 0U : parent->id())};
		return true;
	}

	static void copyTitle(char (&destination)[32], char const* title) {
		uint16_t i = 0;
		if (title != nullptr) {
			while (i < 31U && title[i] != '\0') {
				destination[i] = title[i];
				++i;
			}
		}

		destination[i] = '\0';
	}

	Transport const& _transport;
	char _title[32]{};
	Entry _entries[max_tree_nodes]{};
	uint16_t _count{0};
	bool _open{false};
};

} // namespace lexx::ui
