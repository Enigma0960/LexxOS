#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <lexxos/ui/controls/registry.h>
#include <lexxos/ui/guest_ui_dialog.h>
#include <lexxos/ui/guest_ui_focus_manager.h>
#include <lexxos/ui/guest_ui_layout.h>
#include <lexxos/ui/guest_ui_text_editor.h>
#include <lexxos/ui/guest_ui_tree_layout.h>

namespace lexxos::ui {

class GuestView {
public:
	[[nodiscard]] bool active() const;
	[[nodiscard]] bool dialogOpen() const;
	[[nodiscard]] int scrollOffset() const;
	[[nodiscard]] std::uint16_t focusedId() const;
	bool focusControl(std::uint16_t id);

	void reset();

	// Bit 1 requests a system-managed vertical layout. The ABI still carries
	// x/y/width/height, but the OS resolves them in its private Document copy.
	bool configure(Document const& document, int width, int height);
	bool configureTree(TreeDocument const& document, int width, int height);
	bool configureTreeV3(TreeHeaderV3 const& document, TreeNode const* nodes, int width, int height);
	void preserveControlState(GuestView const& previous);
	bool showDialog(DialogDescriptor const& requested);

	// Only data and disabled state are mutable. Geometry and kind stay fixed.
	bool update(Control const& requested);

	bool setItems(ListItems const& items);
	[[nodiscard]] ListItems const& itemsFor(std::uint16_t id) const;

	bool render(devices::DisplayDevice& display) const;
	bool renderChanges(devices::DisplayDevice& display, GuestView const& previous) const;

	bool key(devices::KeyCode code, char character, Event& event, bool& dirty);

private:
	bool configureResolved(ResolvedDocument const& resolved, int width, int height, int scrollMax, int listIndex);
	void ensureFocusVisible();

private:
	ResolvedDocument _document{};
	std::array<ListItems, maxListStores> _itemStores{};
	int _listIndex{-1};
	bool _active{false};
	int _scrollY{0};
	int _scrollMax{0};
	int _screenHeight{0};
	int _screenWidth{0};
	std::uint8_t _dialogSelection{0};
	bool _dialogOpen{false};
	FocusManager _focus{};
	TextEditor _editor{};
	DialogDescriptor _dialog{};
};

} // namespace lexxos::ui
