#pragma once

#include <lexx/ui/transport.h>

namespace lexx::ui {

// Base class for guest-side widget descriptions. No OS heap, RTTI, virtual
// methods, std::function or per-control syscalls. Window owns no widgets:
// instances must outlive their Window registration.
class Widget {
public:
	Widget(Widget const&) = delete;
	Widget& operator=(Widget const&) = delete;
	Widget(Widget&&) = delete;
	Widget& operator=(Widget&&) = delete;

	[[nodiscard]] uint16_t id() const { return _control.id; }

	[[nodiscard]] Control const& descriptor() const { return _control; }

	[[nodiscard]] bool attached() const { return _attached; }

	Widget& set_enabled(bool enabled) {
		_control.flags = static_cast<uint8_t>(enabled
		        ? (_control.flags & ~flag_disabled)
		        : (_control.flags | flag_disabled));
		(void) synchronize();
		return *this;
	}

	Widget& set_geometry(int16_t x, int16_t y, int16_t width, int16_t height) {
		// Geometry is immutable once the document has been submitted to the OS.
		if (!_attached) {
			_control.x = x;
			_control.y = y;
			_control.width = width;
			_control.height = height;
			_control.flags = static_cast<uint8_t>(_control.flags & ~flag_auto_layout);
		}

		return *this;
	}

	Widget& use_auto_layout(int16_t preferredHeight = 0) {
		if (!_attached) {
			_control.flags = static_cast<uint8_t>(_control.flags | flag_auto_layout);
			_control.height = preferredHeight;
		}
		return *this;
	}

	// Mark a widget as part of the preceding horizontal layout row. The
	// actual geometry remains owned by the system.
	Widget& continue_row() {
		if (!_attached) {
			_control.flags = static_cast<uint8_t>(_control.flags | flag_auto_layout | flag_same_row);
		}

		return *this;
	}

	// Window dispatches semantic OS events to a type-owned handler.
	void dispatch(Event const& event) {
		if (_eventHandler != nullptr) {
			_eventHandler(_eventContext, event);
		}
	}

	// Called only by Window after a successful document show().
	int activate(Transport const& transport) {
		_transport = &transport;
		_attached = true;
		return _attachHandler != nullptr ? _attachHandler(_attachContext) : 0;
	}

	void deactivate() {
		_attached = false;
		_transport = nullptr;
	}

protected:
	explicit Widget(Kind kind, uint16_t id, char const* text = "", uint32_t value = 0)
	    : _control{} {
		_control.id = id;
		_control.kind = static_cast<uint8_t>(kind);
		_control.flags = flag_auto_layout;
		_control.value = value;
		copyText(_control.text, text);
	}

	static void copyText(char (&destination)[32], char const* text) {
		uint32_t length = 0;
		if (text != nullptr) {
			while (length < 31U && text[length] != '\0') {
				destination[length] = text[length];
				++length;
			}

			// Do not leave a truncated trailing UTF-8 sequence.
			if (text[length] != '\0') {
				while (length > 0U && (static_cast<uint8_t>(text[length]) & 0xC0U) == 0x80U) {
					--length;
				}
			}
		}

		destination[length] = '\0';
	}

	void setLocalText(char const* text) { copyText(_control.text, text); }

	void setLocalValue(uint32_t value) { _control.value = value; }

	[[nodiscard]] Transport const* transport() const { return _transport; }

	int synchronize() const {
		return _attached && _transport != nullptr ? _transport->update(_control) : 0;
	}

	using EventHandler = void (*)(void*, Event const&);
	using AttachHandler = int (*)(void*);

	void setEventHandler(EventHandler handler, void* context) {
		_eventHandler = handler;
		_eventContext = context;
	}

	void setAttachHandler(AttachHandler handler, void* context) {
		_attachHandler = handler;
		_attachContext = context;
	}

	Control _control{};

private:
	EventHandler _eventHandler{nullptr};
	void* _eventContext{nullptr};
	AttachHandler _attachHandler{nullptr};
	void* _attachContext{nullptr};
	Transport const* _transport{nullptr};
	bool _attached{false};
};

} // namespace lexx::ui
