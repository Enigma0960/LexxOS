#pragma once

#include <lexx/ui/row.h>
#include <lexx/ui/widget.h>

namespace lexx::ui {

// Non-owning, bounded document builder. Widget objects live in the .lexx app;
// the kernel stores a validated copy of descriptors after show().
class Window {
public:
	Window(char const* title, Transport const& transport)
	    : _transport(transport) { copyTitle(_title, title); }

#if !defined(LEXXOS_UI_NATIVE)
	explicit Window(char const* title)
	    : Window(title, guestTransport) {}
#endif

	Window(Window const&) = delete;
	Window& operator=(Window const&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	// Avoid dereferencing non-owning widget pointers during destruction: users
	// may have declared their Window before their local widget instances.
	~Window() {
		if (_open) {
			(void) close();
		}
	}

	bool add(Widget& widget) {
		if (_open || _count >= max_controls || widget.id() == 0U) {
			return false;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_widgets[i]->id() == widget.id() || _widgets[i] == &widget) {
				return false;
			}
		}

		_widgets[_count++] = &widget;
		return true;
	}

	// Flat ABI with OS-managed horizontal geometry for adjacent widgets.
	bool add(Row const& row) {
		if (_open || row.size() == 0U || _count + row.size() > max_controls) {
			return false;
		}

		for (uint16_t i = 0; i < row.size(); ++i) {
			Widget* item = row.at(i);
			if (item == nullptr || !add(*item)) {
				return false;
			}

			if (i != 0U) {
				item->continue_row();
			}
		}

		return true;
	}

	int show() {
		if (_open) {
			return -1;
		}

		Document document{};
		copyTitle(document.title, _title);
		document.count = _count;
		for (uint16_t i = 0; i < _count; ++i) {
			document.controls[i] = _widgets[i]->descriptor();
		}

		int const status = _transport.show(document);
		if (status != 0) {
			return status;
		}

		_open = true;
		for (uint16_t i = 0; i < _count; ++i) {
			int const attached = _widgets[i]->activate(_transport);
			if (attached != 0) {
				(void) close();
				return attached;
			}
		}

		return 0;
	}

	// Poll and automatically dispatch by stable widget ID.
	// The application never needs a switch on Kind or an if-chain on event.id.
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
			if (_widgets[i]->id() == event.id) {
				_widgets[i]->dispatch(event);
				break;
			}
		}

		return result;
	}

	// Cooperative event loop; callbacks can call window.close().
	int exec(uint32_t intervalMs = 20U) {
		if (!_open) {
			int const result = show();
			if (result != 0) {
				return result;
			}
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
			_widgets[i]->deactivate();
		}

		return _transport.close();
	}

	[[nodiscard]] bool is_open() const { return _open; }

private:
	static void copyTitle(char (&target)[32], char const* source) {
		uint32_t i = 0;
		if (source != nullptr) {
			while (i < 31U && source[i] != '\0') {
				target[i] = source[i];
				++i;
			}
		}

		target[i] = '\0';
	}

	Transport const& _transport;
	char _title[32]{};
	Widget* _widgets[max_controls]{};
	uint16_t _count{0};
	bool _open{false};
};

} // namespace lexx::ui
