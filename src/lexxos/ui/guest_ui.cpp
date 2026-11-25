#include <lexxos/ui/guest_ui.h>

#include <lexxos/ui/guest_ui_clipped_display.h>

namespace lexxos::ui {

bool GuestView::active() const {
	return _active;
}

std::uint16_t GuestView::focusedId() const {
	int const index = _focus.index();
	return _active && index >= 0 ? _document.controls[index].id : 0U;
}

bool GuestView::focusControl(std::uint16_t id) {
	if (!_active || !_focus.setId(_document, id)) {
		return false;
	}

	ensureFocusVisible();
	if (_document.controls[_focus.index()].kind == 4U) {
		_editor.reset(_document.controls[_focus.index()]);
		_editor.adjustViewport(_document.controls[_focus.index()]);
	}

	return true;
}

void GuestView::reset() {
	_active = false;
	_focus = {};
	_editor = {};
	_document = {};
	_itemStores = {};
	_listIndex = -1;
	_scrollY = 0;
	_scrollMax = 0;
	_screenHeight = 0;
	_screenWidth = 0;
	_dialog = {};
	_dialogSelection = 0;
	_dialogOpen = false;
}

bool GuestView::configure(Document const& document, int width, int height) {
	Document resolved = document;
	int scrollMax = 0;
	int listIndex = -1;
	if (!layout::resolve(resolved, width, height, scrollMax, listIndex)) {
		return false;
	}

	_document = {};
	std::memcpy(_document.title, resolved.title, sizeof(_document.title));
	_document.count = resolved.count;
	_document.version = resolved.version;
	for (std::uint16_t i = 0; i < resolved.count; ++i) {
		_document.controls[i] = resolved.controls[i];
	}

	_itemStores = {};
	_listIndex = listIndex;
	_scrollMax = scrollMax;
	_scrollY = 0;
	_screenHeight = height;
	_screenWidth = width;
	_dialog = {};
	_dialogSelection = 0;
	_dialogOpen = false;
	_focus.reset(_document);

	if (_focus.index() >= 0 && _document.controls[_focus.index()].kind == 4U) {
		_editor.reset(_document.controls[_focus.index()]);
		_editor.adjustViewport(_document.controls[_focus.index()]);
	}

	_active = true;
	ensureFocusVisible();
	return true;
}

bool GuestView::configureTree(TreeDocument const& document, int width, int height) {
	ResolvedDocument resolved{};
	int scrollMax = 0;
	int listIndex = -1;
	if (!tree::resolve(document, resolved, width, height, scrollMax, listIndex)) {
		return false;
	}

	return configureResolved(resolved, width, height, scrollMax, listIndex);
}

bool GuestView::configureTreeV3(TreeHeaderV3 const& document, TreeNode const* nodes, int width, int height) {
	ResolvedDocument resolved{};
	int scrollMax = 0;
	int listIndex = -1;
	if (!tree::resolve(document, nodes, resolved, width, height, scrollMax, listIndex)) {
		return false;
	}

	return configureResolved(resolved, width, height, scrollMax, listIndex);
}

bool GuestView::configureResolved(ResolvedDocument const& resolved, int width, int height,
    int scrollMax, int listIndex) {
	std::size_t lists = 0;
	for (std::uint16_t i = 0; i < resolved.count; ++i) {
		auto const* handler = controls::find(resolved.controls[i].kind);
		lists += handler != nullptr && handler->usesListItems ? 1U : 0U;
	}

	if (lists > maxListStores) {
		return false;
	}

	_document = resolved;
	_itemStores = {};
	_listIndex = listIndex;
	_scrollMax = scrollMax;
	_scrollY = 0;
	_screenHeight = height;
	_screenWidth = width;
	_dialog = {};
	_dialogSelection = 0;
	_dialogOpen = false;
	_focus.reset(_document);

	if (_focus.index() >= 0 && _document.controls[_focus.index()].kind == 4U) {
		_editor.reset(_document.controls[_focus.index()]);
		_editor.adjustViewport(_document.controls[_focus.index()]);
	}

	_active = true;
	ensureFocusVisible();
	return true;
}

void GuestView::preserveControlState(GuestView const& previous) {
	if (!_active || !previous._active) {
		return;
	}

	for (std::uint16_t i = 0; i < _document.count; ++i) {
		auto& control = _document.controls[i];
		for (std::uint16_t j = 0; j < previous._document.count; ++j) {
			auto const& existing = previous._document.controls[j];
			if (existing.id != control.id || existing.kind != control.kind) {
				continue;
			}

			std::memcpy(control.text, existing.text, sizeof(control.text));
			control.value = existing.value;
			// Structural patch kind 4 owns the active-tab marker; only disabled
			// state is retained from the previous interactive widget.
			control.flags = (control.flags & ~1U) | (existing.flags & 1U);
			break;
		}
	}

	// Preserve item pages only for surviving list widgets with the same kind.
	for (std::uint16_t i = 0; i < _document.count; ++i) {
		auto const& current = _document.controls[i];
		auto const* handler = controls::find(current.kind);
		if (handler == nullptr || !handler->usesListItems) {
			continue;
		}

		for (std::uint16_t j = 0; j < previous._document.count; ++j) {
			auto const& old = previous._document.controls[j];
			if (old.id != current.id || old.kind != current.kind) {
				continue;
			}

			for (auto const& store : previous._itemStores) {
				if (store.id == current.id) {
					(void) setItems(store);
					break;
				}
			}

			break;
		}
	}

	std::uint16_t const id = previous.focusedId();
	if (id != 0U) {
		(void) focusControl(id);
	}

	if (_focus.index() >= 0 && _document.controls[_focus.index()].kind == 4U) {
		_editor.clamp(_document.controls[_focus.index()]);
		_editor.adjustViewport(_document.controls[_focus.index()]);
	}

	ensureFocusVisible();
}

bool GuestView::showDialog(DialogDescriptor const& requested) {
	if (!_active || _dialogOpen || _screenWidth < 96 || _screenHeight < 120 || !dialog::validate(requested)) {
		return false;
	}

	_dialog = requested;
	_dialogOpen = true;
	// The safe default for a question is Cancel, not an accidental OK.
	_dialogSelection = requested.buttons == 2U ? 1U : 0U;
	return true;
}

bool GuestView::dialogOpen() const {
	return _dialogOpen;
}

int GuestView::scrollOffset() const {
	return _scrollY;
}

bool GuestView::update(Control const& requested) {
	if (!_active || requested.text[31] != '\0' || (requested.flags & ~layout::allowedFlags) != 0U) {
		return false;
	}

	for (std::uint16_t i = 0; i < _document.count; ++i) {
		auto& control = _document.controls[i];
		if (control.id != requested.id) {
			continue;
		}

		if (control.kind != requested.kind) {
			return false;
		}

		std::memcpy(control.text, requested.text, sizeof(control.text));

		if (_focus.index() == static_cast<int>(i) && control.kind == 4U) {
			_editor.clamp(control);
			_editor.adjustViewport(control);
		}

		if ((requested.flags & layout::tabActive) != 0U && control.kind != 2U) {
			return false;
		}

		control.flags = (control.flags & (layout::autoLayout | layout::sameRow))
		    | (requested.flags & (1U | layout::tabActive));
		control.value = requested.value;
		controls::find(control.kind)->normalize(control, itemsFor(control.id).count, false);

		if (_focus.index() == static_cast<int>(i) && !controls::canFocus(control)) {
			(void) _focus.moveSequential(_document, 1);
			(void) _focus.reconcile(_document);

			if (_focus.index() >= 0 && _document.controls[_focus.index()].kind == 4U) {
				_editor.reset(_document.controls[_focus.index()]);
				_editor.adjustViewport(_document.controls[_focus.index()]);
			}

			ensureFocusVisible();
		}

		return true;
	}

	return false;
}

ListItems const& GuestView::itemsFor(std::uint16_t id) const {
	for (auto const& store : _itemStores) {
		if (store.id == id) {
			return store;
		}
	}

	static ListItems const empty{};
	return empty;
}

bool GuestView::setItems(ListItems const& items) {
	if (!_active || items.count > 8U) {
		return false;
	}

	Control* target = nullptr;
	for (std::uint16_t i = 0; i < _document.count; ++i) {
		auto& control = _document.controls[i];
		auto const* handler = controls::find(control.kind);
		if (control.id == items.id && handler != nullptr && handler->usesListItems) {
			target = &control;
			break;
		}
	}

	if (target == nullptr) {
		return false;
	}

	for (std::uint16_t i = 0; i < items.count; ++i) {
		if (items.entries[i][31] != '\0') {
			return false;
		}
	}

	ListItems* store = nullptr;
	for (auto& itemStore : _itemStores) {
		if (itemStore.id == items.id) {
			store = &itemStore;
			break;
		}
	}

	if (store == nullptr) {
		for (auto& itemStore : _itemStores) {
			if (itemStore.id == 0U) {
				store = &itemStore;
				break;
			}
		}
	}

	if (store == nullptr) {
		return false;
	}

	*store = items;
	controls::find(target->kind)->normalize(*target, items.count, false);
	return true;
}

bool GuestView::render(devices::DisplayDevice& display) const {
	if (!_active || !display.beginFrame()) {
		return false;
	}

	bool ok = display.fillRgb565({0, 0, display.width(), display.height()}, canvas)
	    && display.fillRgb565({2, 2, display.width() - 4, 28}, titleColor)
	    && drawText(display, 8, 8, _document.title, textColor, titleColor, display.width() - 16);
	for (std::uint16_t i = 0; ok && i < _document.count; ++i) {
		Control control = _document.controls[i];
		if ((control.flags & layout::autoLayout) != 0U) {
			int const visibleY = control.y - _scrollY;
			// Clipping is not yet provided by DisplayDevice. Render only
			// completely visible widgets and never write past the panel.
			if (visibleY < layout::top || visibleY + control.height > display.height() - 4) {
				continue;
			}

			control.y = static_cast<std::int16_t>(visibleY);
		}

		auto const* handler = controls::find(control.kind);
		bool const editing = static_cast<int>(i) == _focus.index() && !_dialogOpen && control.kind == 4U;
		ok = editing ? _editor.render(display, control)
		             : handler != nullptr && handler->render(display, control, itemsFor(control.id), static_cast<int>(i) == _focus.index() && !_dialogOpen);
	}

	if (ok && _scrollMax > 0) {
		int const barHeight = display.height() - layout::top - 6;
		int const thumbHeight = std::max(10, (barHeight * barHeight) / (barHeight + _scrollMax));
		int const travel = barHeight - thumbHeight;
		int const thumbY = layout::top + (travel * _scrollY) / _scrollMax;
		ok = display.fillRgb565({display.width() - 5, layout::top, 3, barHeight}, titleColor)
		    && display.fillRgb565({display.width() - 5, thumbY, 3, thumbHeight}, accent);
	}

	if (ok && _dialogOpen) {
		ok = dialog::render(display, _dialog, _dialogSelection);
	}

	bool const committed = display.endFrame();
	return ok && committed;
}

bool GuestView::renderChanges(devices::DisplayDevice& display, GuestView const& previous) const {
	if (!display.supportsPartialUpdates() || !_active || !previous._active || _screenWidth != previous._screenWidth || _screenHeight != previous._screenHeight || _scrollY != previous._scrollY || _scrollMax != previous._scrollMax || _document.version != previous._document.version || _dialogOpen != previous._dialogOpen || _dialogOpen || std::memcmp(_document.title, previous._document.title, sizeof(_document.title)) != 0) {
		return render(display);
	}

	devices::Rect regions[8]{};
	std::uint16_t regionCount = 0;
	auto append = [&](Control const& control) {
		int const y = control.y - ((control.flags & layout::autoLayout) != 0U ? _scrollY : 0);
		int const left = std::max(0, static_cast<int>(control.x));
		int const top = std::max(0, y - ((control.flags & layout::tabActive) != 0U && control.kind == 2U ? 3 : 0));
		int const right = std::min(display.width(), static_cast<int>(control.x) + control.width);
		int const bottom = std::min(display.height(), y + control.height);
		if (right <= left || bottom <= top) {
			return true;
		}

		devices::Rect const area{left, top, right - left, bottom - top};
		for (std::uint16_t i = 0; i < regionCount; ++i) {
			auto& rect = regions[i];
			if (area.x <= rect.x + rect.width && rect.x <= area.x + area.width && area.y <= rect.y + rect.height && rect.y <= area.y + area.height) {
				int const x = std::min(rect.x, area.x);
				int const yy = std::min(rect.y, area.y);
				rect = {x, yy, std::max(rect.x + rect.width, area.x + area.width) - x,
				    std::max(rect.y + rect.height, area.y + area.height) - yy};
				return true;
			}
		}

		if (regionCount == 8U) {
			return false;
		}

		regions[regionCount++] = area;
		return true;
	};

	for (std::uint16_t i = 0; i < previous._document.count; ++i) {
		auto const& before = previous._document.controls[i];
		Control const* after = nullptr;
		for (std::uint16_t j = 0; j < _document.count; ++j) {
			if (_document.controls[j].id == before.id && _document.controls[j].kind == before.kind) {
				after = &_document.controls[j];
				break;
			}
		}

		bool const changed = after == nullptr || std::memcmp(&before, after, sizeof(Control)) != 0 || (before.kind == 4U && before.id == focusedId() && (_editor.cursor() != previous._editor.cursor() || _editor.first() != previous._editor.first())) || (before.id == previous.focusedId()) != (before.id == focusedId()) || (controls::find(before.kind)->usesListItems && std::memcmp(&itemsFor(before.id), &previous.itemsFor(before.id), sizeof(ListItems)) != 0);
		if (changed && !append(before)) {
			return render(display);
		}
	}

	for (std::uint16_t i = 0; i < _document.count; ++i) {
		auto const& after = _document.controls[i];
		Control const* before = nullptr;
		for (std::uint16_t j = 0; j < previous._document.count; ++j) {
			if (previous._document.controls[j].id == after.id && previous._document.controls[j].kind == after.kind) {
				before = &previous._document.controls[j];
				break;
			}
		}

		bool const changed = before == nullptr || std::memcmp(before, &after, sizeof(Control)) != 0 || (after.kind == 4U && after.id == focusedId() && (_editor.cursor() != previous._editor.cursor() || _editor.first() != previous._editor.first())) || (after.id == previous.focusedId()) != (after.id == focusedId()) || (controls::find(after.kind)->usesListItems && std::memcmp(&itemsFor(after.id), &previous.itemsFor(after.id), sizeof(ListItems)) != 0);
		if (changed && !append(after)) {
			return render(display);
		}
	}

	if (regionCount == 0U) {
		return true;
	}

	if (!display.beginFrame()) {
		return false;
	}

	bool ok = true;
	for (std::uint16_t i = 0; i < regionCount && ok; ++i) {
		ClippedDisplay clip{display, regions[i]};
		ok = render(clip);
	}

	bool const completed = display.endFrame();
	return ok && completed;
}

bool GuestView::key(devices::KeyCode code, char character, Event& event, bool& dirty) {
	event = {};
	dirty = false;
	if (!_active) {
		return false;
	}

	if (_dialogOpen) {
		bool const consumed = dialog::key(_dialog, _dialogSelection, code, character, event, dirty);
		if (event.type == 4U) {
			_dialogOpen = false;
		}

		return consumed;
	}

	if (code == devices::KeyCode::Escape || character == 0x1BU) {
		event.type = 3;
		return true;
	}

	if (code == devices::KeyCode::Tab || character == '\t') {
		if (_focus.moveSequential(_document, 1)) {
			if (_document.controls[_focus.index()].kind == 4U) {
				_editor.reset(_document.controls[_focus.index()]);
				_editor.adjustViewport(_document.controls[_focus.index()]);
			}

			ensureFocusVisible();
			dirty = true;
		}

		return true;
	}

	if (_focus.index() < 0) {
		return false;
	}

	auto& control = _document.controls[_focus.index()];

	auto const* handler = controls::find(control.kind);
	if (handler == nullptr) {
		return false;
	}

	event.id = control.id;
	if (control.kind == 4U && (code == devices::KeyCode::Left || code == devices::KeyCode::Right || code == devices::KeyCode::Home || code == devices::KeyCode::End || code == devices::KeyCode::Backspace || code == devices::KeyCode::Delete || character == '\b' || static_cast<unsigned char>(character) == 0x7FU || (character >= 32 && character <= 126))) {
		bool const consumed = _editor.key(control, code, character, event, dirty);
		_editor.adjustViewport(control);
		return consumed;
	}

	bool const horizontal = code == devices::KeyCode::Left || code == devices::KeyCode::Right;
	bool const vertical = code == devices::KeyCode::Up || code == devices::KeyCode::Down;
	// Sliders edit their value horizontally; a populated list edits its
	// selection vertically. All other arrow keys navigate spatially.
	bool const sliderEditing = horizontal && control.kind == 6U;
	bool const listEditing = vertical && handler->usesListItems && itemsFor(control.id).count != 0U;
	if ((horizontal || vertical) && !sliderEditing && !listEditing) {
		if (_focus.moveSpatial(_document, code)) {
			if (_document.controls[_focus.index()].kind == 4U) {
				_editor.reset(_document.controls[_focus.index()]);
				_editor.adjustViewport(_document.controls[_focus.index()]);
			}

			ensureFocusVisible();
			dirty = true;
		}

		return true;
	}

	return handler->key(control, itemsFor(control.id), code, character, event, dirty);
}

void GuestView::ensureFocusVisible() {
	if (_focus.index() < 0 || _scrollMax == 0) {
		return;
	}

	auto const& control = _document.controls[_focus.index()];
	if ((control.flags & layout::autoLayout) == 0U) {
		return;
	}

	if (control.y < _scrollY + layout::top) {
		_scrollY = std::max(0, control.y - layout::top);
	}

	if (control.y + control.height > _scrollY + _screenHeight - 4) {
		_scrollY = control.y + control.height - (_screenHeight - 4);
	}

	_scrollY = std::clamp(_scrollY, 0, _scrollMax);
}

} // namespace lexxos::ui
