#include <cassert>
#include <cstring>

#include <lexxos/ui/guest_ui.h>

int main() {
	using lexxos::ui::Control;
	using lexxos::ui::Document;
	using lexxos::ui::Event;
	using lexxos::ui::glyph;
	using lexxos::ui::GuestView;
	using lexxos::ui::ListItems;
	using lexxos::ui::nextCodepoint;
	GuestView view{};
	Document data{};
	std::memcpy(data.title, "Settings", 9);
	data.count = 3;
	data.controls[0] = {1, 1, 0, 8, 42, 180, 16, 0, "Title"};
	data.controls[1] = {2, 3, 0, 8, 70, 180, 28, 0, "Enabled"};
	data.controls[2] = {3, 2, 0, 8, 112, 180, 28, 0, "Save"};
	assert(view.configure(data, 320, 320));
	Event event{};
	bool dirty = false;
	assert(view.key(lexxos::devices::KeyCode::Enter, '\n', event, dirty));
	assert(event.type == 2 && event.id == 2 && event.value == 1 && dirty);
	assert(view.key(lexxos::devices::KeyCode::Tab, '\0', event, dirty) && dirty);
	assert(view.key(lexxos::devices::KeyCode::Enter, '\n', event, dirty));
	assert(event.type == 1 && event.id == 3);
	assert(view.key(lexxos::devices::KeyCode::Escape, '\0', event, dirty));
	assert(event.type == 3);
	auto duplicate = data;
	duplicate.controls[2].id = duplicate.controls[1].id;
	assert(!view.configure(duplicate, 320, 320));
	auto overrun = data;
	overrun.controls[1].width = 340;
	assert(!view.configure(overrun, 320, 320));
	char const* utf8 = "Яё";
	auto* ptr = utf8;
	auto const* end = ptr + std::strlen(utf8);
	assert(nextCodepoint(ptr, end) == 0x042F);
	assert(nextCodepoint(ptr, end) == 0x0451);
	assert(glyph(0x042F).codepoint == 0x042F);

	// Auto-layout is resolved by the system without writing back guest memory.
	Document automatic{};
	std::memcpy(automatic.title, "Layout", 7);
	automatic.count = 4;
	automatic.controls[0] = {1, 1, 2, 0, 0, 0, 0, 0, "Caption"};
	automatic.controls[1] = {2, 7, 2, 0, 0, 0, 58, 0, ""};
	automatic.controls[2] = {3, 6, 2, 0, 0, 0, 0, 25, ""};
	automatic.controls[3] = {4, 5, 2, 0, 0, 0, 0, 25, ""};
	assert(view.configure(automatic, 320, 320));
	assert(automatic.controls[0].width == 0); // no guest-side mutation
	ListItems items{};
	items.id = 2;
	items.count = 3;
	std::memcpy(items.entries[0], "First", 6);
	std::memcpy(items.entries[1], "Second", 7);
	std::memcpy(items.entries[2], "Third", 6);
	assert(view.setItems(items));
	assert(!view.setItems(ListItems{}));
	assert(view.key(lexxos::devices::KeyCode::Down, '\0', event, dirty));
	assert(event.id == 2 && event.type == 2 && event.value == 1 && dirty);
	assert(view.key(lexxos::devices::KeyCode::Enter, '\n', event, dirty));
	assert(event.type == 1 && event.value == 1);
	assert(view.key(lexxos::devices::KeyCode::Tab, '\0', event, dirty));
	assert(view.key(lexxos::devices::KeyCode::Right, '\0', event, dirty));
	assert(event.id == 3 && event.type == 2 && event.value == 30);
	Control progress{4, 5, 2, 0, 0, 0, 0, 75, ""};
	assert(view.update(progress));
	assert(!view.update(Control{99, 5, 0, 0, 0, 0, 0, 1, ""}));
	assert(!view.configure(automatic, 96, 64)); // a single control must fit the viewport

	// Configure must preserve a ListView's requested initial selection until
	// the application supplies the list contents. Reinitializing at configure
	// time would incorrectly reset a valid selected index to zero.
	Document initialSelection = automatic;
	initialSelection.controls[1].value = 2;
	assert(view.configure(initialSelection, 320, 320));
	assert(view.setItems(items));
	assert(view.key(lexxos::devices::KeyCode::Enter, 0, event, dirty));
	assert(event.type == 1 && event.id == 2 && event.value == 2);

	// The system registry has exactly one independent implementation for each
	// legacy ABI control type. Unknown kinds must never be dispatched.
	assert(lexxos::ui::controls::find(0) == nullptr);
	assert(lexxos::ui::controls::find(8) == nullptr);
	for (std::uint8_t kind = 1; kind <= 7; ++kind) {
		auto const* handler = lexxos::ui::controls::find(kind);
		assert(handler != nullptr && handler->render != nullptr);
		assert(handler->key != nullptr && handler->normalize != nullptr);
	}

	// Per-control normalization and events are tested without GuestView:
	// the handlers have no shared mutable state or dependency on each other.
	Control checkbox{8, 3, 0, 0, 0, 80, 28, 7, "Checked"};
	lexxos::ui::controls::find(checkbox.kind)->normalize(checkbox, 0U, false);
	assert(checkbox.value == 1);
	Event change{};
	bool rerender = false;
	assert(lexxos::ui::controls::find(checkbox.kind)->key(checkbox, {}, lexxos::devices::KeyCode::Space, ' ', change, rerender));
	assert(checkbox.value == 0 && change.type == 2 && change.value == 0 && rerender);

	Control slider{9, 6, 0, 0, 0, 100, 28, 110, ""};
	lexxos::ui::controls::find(slider.kind)->normalize(slider, 0U, false);
	assert(slider.value == 100);
	change = {};
	rerender = false;
	assert(lexxos::ui::controls::find(slider.kind)->key(slider, {}, lexxos::devices::KeyCode::Left, 0, change, rerender));
	assert(slider.value == 95 && change.type == 2 && change.value == 95 && rerender);

	Control edit{10, 4, 0, 0, 0, 100, 28, 0, "AЯ"};
	change = {};
	rerender = false;
	assert(lexxos::ui::controls::find(edit.kind)->key(edit, {}, lexxos::devices::KeyCode::Backspace, '\b', change, rerender));
	assert(std::strcmp(edit.text, "A") == 0 && change.type == 2 && rerender);
	change = {};
	rerender = false;
	assert(lexxos::ui::controls::find(edit.kind)->key(edit, {}, lexxos::devices::KeyCode::Unknown, 'z', change, rerender));
	assert(std::strcmp(edit.text, "Az") == 0 && rerender);

	// S5.4: layout rows share an identical vertical origin. Continuation
	// without an initial auto-layout control is rejected by the OS.
	Document rows{};
	std::memcpy(rows.title, "Rows", 5);
	rows.count = 3;
	rows.controls[0] = {1, 2, 2, 0, 0, 0, 0, 0, "OK"};
	rows.controls[1] = {2, 2, 6, 0, 0, 0, 0, 0, "Cancel"};
	rows.controls[2] = {3, 2, 2, 0, 0, 0, 0, 0, "Next"};
	int scrollMax = 0, listIndex = -1;
	assert(lexxos::ui::layout::resolve(rows, 320, 320, scrollMax, listIndex));
	assert(rows.controls[0].y == rows.controls[1].y);
	assert(rows.controls[1].x > rows.controls[0].x);
	assert(rows.controls[2].y > rows.controls[0].y);
	assert(scrollMax == 0);
	rows.controls[0].flags = 6;
	assert(!lexxos::ui::layout::resolve(rows, 320, 320, scrollMax, listIndex));

	// Oversized forms scroll to the newly focused widget rather than failing.
	Document longForm{};
	std::memcpy(longForm.title, "Scrolling", 10);
	longForm.count = 8;
	for (std::uint16_t index = 0; index < 8; ++index) {
		longForm.controls[index] = {static_cast<std::uint16_t>(index + 1U), 2, 2, 0, 0, 0, 58, 0, "Action"};
	}

	assert(view.configure(longForm, 320, 320));
	assert(view.scrollOffset() == 0);
	for (int i = 0; i < 7; ++i) {
		assert(view.key(lexxos::devices::KeyCode::Tab, '\t', event, dirty));
	}

	assert(view.scrollOffset() > 0);

	// A modal message box consumes keys and restores the underlying window.
	lexxos::ui::DialogDescriptor box{};
	std::memcpy(box.title, "Confirm", 8);
	std::memcpy(box.message, "Save settings?", 15);
	box.id = 42;
	box.kind = 4;
	box.buttons = 2;
	assert(view.showDialog(box));
	assert(view.dialogOpen());
	assert(!view.showDialog(box));
	assert(view.key(lexxos::devices::KeyCode::Enter, '\r', event, dirty));
	assert(event.type == 4 && event.id == 42 && event.value == 2 && dirty);
	assert(!view.dialogOpen());
	assert(view.showDialog(box));
	assert(view.key(lexxos::devices::KeyCode::Left, 0, event, dirty));
	assert(event.type == 0 && dirty);
	assert(view.key(lexxos::devices::KeyCode::Enter, '\r', event, dirty));
	assert(event.type == 4 && event.value == 1 && dirty);
	assert(view.showDialog(box));
	assert(view.key(lexxos::devices::KeyCode::Escape, 0, event, dirty));
	assert(event.type == 4 && event.value == 2);
	box.kind = 3;
	box.buttons = 1;
	assert(view.showDialog(box));
	assert(view.key(lexxos::devices::KeyCode::Escape, 0, event, dirty));
	assert(event.type == 4 && event.value == 1);
	box.kind = 5;
	assert(!view.showDialog(box));
}
