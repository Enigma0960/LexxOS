#include <cassert>
#include <cstring>

#include <lexx/ui/button.h>
#include <lexx/ui/tree_window.h>
#include <lexx/ui/tree_window_v3.h>
#include <lexxos/ui/controls/text_field.h>
#include <lexxos/ui/guest_ui.h>
#include <lexxos/ui/guest_ui_tree_layout.h>
#include <lexxos/ui/guest_ui_tree_patch.h>
#include <lexxos/ui/ui_service.h>

namespace {

struct State {
	int shown{0};
	int updateBatches{0};
	std::uint16_t lastUpdatedCount{0};
	int shownV3{0};
	std::uint16_t lastV3Count{0};
	int closed{0};
	lexx::ui::TreeDocument document{};
};

int showLegacy(void*, lexx::ui::Document const&) {
	return -1;
}

int poll(void*, lexx::ui::Event&) {
	return 0;
}

int close(void* context) {
	++static_cast<State*>(context)->closed;
	return 0;
}

int update(void*, lexx::ui::Control const&) {
	return 0;
}

int setItems(void*, lexx::ui::ListItems const&) {
	return 0;
}

int dialog(void*, lexx::ui::DialogDescriptor const&) {
	return 0;
}

void sleep(void*, lexx::uint32_t) {
}

int showTree(void* context, lexx::ui::TreeDocument const& doc) {
	auto& state = *static_cast<State*>(context);
	state.document = doc;
	++state.shown;
	return 0;
}

int queryCapabilities(void*, lexx::ui::UiCapabilities& caps) {
	caps = {3U, 32U, 32U, 1U};
	return 0;
}

int showTreeV3(void* context, lexx::ui::TreeHeaderV3 const& header,
    lexx::ui::TreeNode const* nodes) {
	auto& state = *static_cast<State*>(context);
	assert(header.version == 3U && nodes != nullptr);
	++state.shownV3;
	state.lastV3Count = header.count;
	return 0;
}

int patchTreeV3(void*, lexx::ui::TreePatchHeader const& header,
    lexx::ui::TreePatchOperation const* operations) {
	return header.version == 1U && header.count == 1U && operations != nullptr && operations[0].kind == 2U && operations[0].id == 1U ? 0 : -1;
}

int updateTreeBatch(void* context, lexx::ui::TreeUpdateHeader const& header,
    lexx::ui::Control const* controls) {
	auto& state = *static_cast<State*>(context);
	assert(header.version == 1U && controls != nullptr && header.count > 0U);
	++state.updateBatches;
	state.lastUpdatedCount = header.count;
	return 0;
}

void batchedUpdateTests() {
	State state{};
	lexx::ui::Transport::Operations const operations{
	    &showLegacy, &poll, &close, &update, &setItems, &dialog, &sleep,
	    &showTree, &queryCapabilities, &showTreeV3, &patchTreeV3, &updateTreeBatch};
	lexx::ui::Transport transport{&state, operations};
	lexx::ui::Column root{100U};
	lexx::ui::Column nested{101U};
	lexx::ui::Button first{1U, "First"};
	lexx::ui::Button second{2U, "Second"};
	lexx::ui::TreeWindowV3<8> window{"Batch update", transport};
	assert(window.add(root));
	assert(window.add(nested, &root));
	assert(window.add(first, &nested));
	assert(window.add(second, &root));
	assert(window.show() == 0);
	assert(window.update(root) == 0 && state.updateBatches == 1 && state.lastUpdatedCount == 2U);
	assert(window.update(nested) == 0 && state.updateBatches == 2 && state.lastUpdatedCount == 1U);
	assert(window.update(999U) < 0 && state.updateBatches == 2);
	assert(window.close() == 0);
}

void layoutTests() {
	State state{};
	lexx::ui::Transport::Operations const operations{
	    &showLegacy, &poll, &close, &update, &setItems, &dialog, &sleep, &showTree};
	lexx::ui::Transport transport{&state, operations};
	lexx::ui::Column root{100};
	lexx::ui::Column left{101};
	lexx::ui::RowContainer row{102};
	lexx::ui::Button buttons[10] = {
	    { 1,   "One"},
	    { 2,   "Two"},
	    { 3, "Three"},
	    { 4,  "Four"},
	    { 5,  "Five"},
	    { 6,   "Six"},
	    { 7, "Seven"},
	    { 8, "Eight"},
	    { 9,  "Nine"},
	    {10,   "Ten"}
    };
	lexx::ui::TreeWindow window{"S5.6", transport};
	assert(!window.add(buttons[0], &root));
	assert(window.add(root));
	assert(window.add(left, &root));
	assert(window.add(row, &root));
	for (int i = 0; i < 8; ++i) {
		assert(window.add(buttons[i], &left));
	}

	for (int i = 8; i < 10; ++i) {
		assert(window.add(buttons[i], &row));
	}

	assert(window.show() == 0);
	assert(state.shown == 1 && state.document.count == 13);

	lexxos::ui::TreeDocument native{};
	static_assert(sizeof(native) == sizeof(state.document));
	std::memcpy(&native, &state.document, sizeof(native));
	lexxos::ui::ResolvedDocument resolved{};
	int scrollMax = 0;
	int listIndex = -1;

	if constexpr (lexxos::ui::maxResolvedControls < 10U) {
		// The fixed v2 wire document has ten leaves; a smaller host pool rejects it.
		assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
		assert(window.close() == 0 && state.closed == 1);
		return;
	}

	assert(lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
	assert(resolved.count == 10 && listIndex == -1);
	assert(lexxos::ui::maxResolvedControls == lexxos::runtime::standardRuntimeProfile.ui.maxResolvedControls);
	assert(scrollMax > 0);
	assert(resolved.controls[0].x == resolved.controls[7].x);
	assert(resolved.controls[8].x < resolved.controls[9].x);
	assert(resolved.controls[8].y == resolved.controls[9].y);

	lexxos::ui::UiBudget budget{};
	assert(budget.valid() && budget.maxNodes == lexxos::ui::treeNodeCapacity);
	budget.maxControls = 0U;
	assert(!budget.valid());
	budget.maxControls = lexxos::ui::maxResolvedControls + 1U;
	assert(!budget.valid());
	budget.maxControls = lexxos::ui::maxResolvedControls;
	budget.maxNodes = static_cast<std::uint16_t>(lexxos::ui::treeNodeCapacity + 1U);
	assert(!budget.valid());

	// Reject malformed parent references, duplicate IDs, unsupported fields,
	// and self-referential nodes without ever modifying an active window.
	native.nodes[1].parentId = 999;
	assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
	native.nodes[1].parentId = 100;
	native.nodes[2].control.id = 100;
	assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
	native.nodes[2].control.id = 102;
	native.nodes[3].reserved = 1;
	assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
	native.nodes[3].reserved = 0;
	native.version = 1;
	assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));
	native.version = 2;
	native.count = lexxos::ui::maxTreeNodes + 1U;
	assert(!lexxos::ui::tree::resolve(native, resolved, 320, 320, scrollMax, listIndex));

	lexxos::ui::TreeDocument deep{};
	deep.count = 6;
	for (std::uint16_t index = 0; index < 5; ++index) {
		deep.nodes[index].control.id = static_cast<std::uint16_t>(100U + index);
		deep.nodes[index].control.kind = 8U;
		deep.nodes[index].control.flags = 2U;
		deep.nodes[index].parentId = index == 0 ? 0U : static_cast<std::uint16_t>(99U + index);
	}

	deep.nodes[5].control.id = 1;
	deep.nodes[5].control.kind = 1;
	deep.nodes[5].control.flags = 2U;
	deep.nodes[5].parentId = 104;
	assert(!lexxos::ui::tree::resolve(deep, resolved, 320, 320, scrollMax, listIndex));
	assert(window.close() == 0 && state.closed == 1);
}

void horizontalFocusTests() {
	lexxos::ui::TreeDocument document{};
	document.count = 4;
	document.nodes[0].control.id = 100U;
	document.nodes[0].control.kind = 9U;
	document.nodes[0].control.flags = 2U;
	document.nodes[1].parentId = 100U;
	document.nodes[1].control.id = 1U;
	document.nodes[1].control.kind = 2U;
	document.nodes[1].control.flags = 2U;
	document.nodes[2].parentId = 100U;
	document.nodes[2].control.id = 2U;
	document.nodes[2].control.kind = 2U;
	document.nodes[2].control.flags = 2U;
	document.nodes[3].control.id = 3U;
	document.nodes[3].control.kind = 2U;
	document.nodes[3].control.flags = 2U;

	lexxos::ui::GuestView view{};
	assert(view.configureTree(document, 320, 320));
	lexxos::ui::Event event{};
	bool dirty = false;
	auto press = [&](lexxos::devices::KeyCode key) {
		assert(view.key(key, '\0', event, dirty));
	};
	auto activate = [&](std::uint16_t id) {
		press(lexxos::devices::KeyCode::Enter);
		assert(event.type == 1U && event.id == id);
	};

	activate(1U);
	press(lexxos::devices::KeyCode::Right);
	assert(dirty);
	activate(2U);
	press(lexxos::devices::KeyCode::Right);
	activate(2U); // Do not jump to the button in another row.
	press(lexxos::devices::KeyCode::Left);
	activate(1U);
	press(lexxos::devices::KeyCode::Down);
	activate(3U); // Geometry, not document order, determines vertical focus.
}

void spatialFocusTests() {
	using lexxos::devices::KeyCode;
	lexxos::ui::ResolvedDocument doc{};
	doc.count = 6;
	// Two rows of buttons, deliberately shuffled in registration order.
	auto set = [&](int index, int x, int y, int width, int kind = 2) {
		auto& c = doc.controls[index];
		c.id = static_cast<std::uint16_t>(index + 1);
		c.kind = static_cast<std::uint8_t>(kind);
		c.x = static_cast<std::int16_t>(x);
		c.y = static_cast<std::int16_t>(y);
		c.width = static_cast<std::int16_t>(width);
		c.height = 28;
	};
	set(0, 10, 40, 140);
	set(1, 170, 90, 140);
	set(2, 170, 40, 140);
	set(3, 10, 90, 140);
	set(4, 10, 140, 140, 6); // Slider is a valid navigation target.
	set(5, 170, 140, 140);
	doc.controls[5].flags = 1U; // Disabled targets are skipped.

	auto next = [&](int index, KeyCode direction) {
		return lexxos::ui::focus::spatialNeighbor(doc, index, direction);
	};

	assert(next(0, KeyCode::Right) == 2);
	assert(next(2, KeyCode::Down) == 1);
	assert(next(1, KeyCode::Left) == 3);
	assert(next(3, KeyCode::Up) == 0);
	assert(next(3, KeyCode::Down) == 4);
	assert(next(4, KeyCode::Right) == -1);
	assert(next(0, KeyCode::Up) == -1);

	// A slider consumes Left/Right to adjust its value instead of moving focus.
	lexxos::ui::Document legacy{};
	legacy.count = 2;
	legacy.controls[0] = doc.controls[4];
	legacy.controls[0].x = 10;
	legacy.controls[0].y = 40;
	legacy.controls[0].value = 50;
	legacy.controls[1] = doc.controls[2];
	legacy.controls[1].x = 170;
	legacy.controls[1].y = 40;
	lexxos::ui::GuestView view{};
	assert(view.configure(legacy, 320, 320));
	lexxos::ui::Event event{};
	bool dirty = false;
	assert(view.key(KeyCode::Right, '\0', event, dirty));
	assert(dirty && event.type == 2U && event.id == legacy.controls[0].id && event.value == 55U);
	assert(view.key(KeyCode::Tab, '\0', event, dirty));
	assert(view.key(KeyCode::Left, '\0', event, dirty));
	assert(view.key(KeyCode::Right, '\0', event, dirty));
	assert(event.type == 2U && event.value == 60U);
}

void focusManagerAndCaretTests() {
	using lexxos::devices::KeyCode;
	lexxos::ui::ResolvedDocument doc{};
	doc.count = 3;
	for (int i = 0; i < 3; ++i) {
		auto& c = doc.controls[i];
		c.id = static_cast<std::uint16_t>(i + 1);
		c.kind = i == 0 ? 4U : 2U;
		c.x = static_cast<std::int16_t>(10 + 150 * (i == 2));
		c.y = static_cast<std::int16_t>(40 + 50 * (i != 0));
		c.width = 130;
		c.height = 28;
	}

	lexxos::ui::FocusManager focus{};
	focus.reset(doc);
	assert(focus.index() == 0);
	assert(focus.setId(doc, 3U) && focus.index() == 2);
	assert(focus.moveSpatial(doc, KeyCode::Left) && focus.index() == 1);
	assert(focus.moveSequential(doc, 1) && focus.index() == 2);
	doc.controls[2].flags = 1U;
	assert(focus.reconcile(doc) && focus.index() == 0);
	assert(!focus.setId(doc, 3U));
	assert(!focus.setId(doc, 99U));

	lexxos::ui::Control field{};
	field.x = 10;
	field.y = 40;
	field.width = 100;
	field.height = 28;
	std::strcpy(field.text, "Test");
	assert(lexxos::ui::controls::text_field::caretX(field) == field.x + 5 + 4 * static_cast<int>(lexxos::ui::font::cellWidth));
	std::strcpy(field.text, "abcdefghijklmnopqrstuvwxyz12345");
	assert(lexxos::ui::controls::text_field::caretX(field) + 2 <= field.x + field.width);
	char const utf8[] = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	assert(lexxos::ui::controls::text_field::visibleSuffix(utf8, 2) == utf8 + 8);
}

void textEditingTests() {
	using lexxos::devices::KeyCode;
	lexxos::ui::Control field{};
	field.id = 77;
	field.kind = 4;
	field.width = 46;
	field.height = 28;
	std::strcpy(field.text,
	    "A\xD0\xAF"
	    "B"); // A, Cyrillic Ya, B
	lexxos::ui::TextEditor editor{};
	editor.reset(field);
	editor.adjustViewport(field);
	lexxos::ui::Event event{};
	bool dirty = false;
	assert(editor.key(field, KeyCode::Left, 0, event, dirty));
	assert(editor.cursor() == 3 && dirty);
	dirty = false;
	assert(editor.key(field, KeyCode::Left, 0, event, dirty));
	assert(editor.cursor() == 1); // Never split a UTF-8 codepoint.
	dirty = false;
	assert(editor.key(field, KeyCode::Delete, 0, event, dirty));
	assert(std::strcmp(field.text, "AB") == 0 && event.type == 2 && event.id == 77);
	dirty = false;
	assert(editor.key(field, KeyCode::Unknown, 'X', event, dirty));
	assert(std::strcmp(field.text, "AXB") == 0 && editor.cursor() == 2);
	dirty = false;
	assert(editor.key(field, KeyCode::Home, 0, event, dirty));
	assert(editor.cursor() == 0);
	dirty = false;
	assert(!editor.key(field, KeyCode::Backspace, 0, event, dirty));
	assert(!dirty && std::strcmp(field.text, "AXB") == 0);
	assert(editor.key(field, KeyCode::End, 0, event, dirty));
	assert(editor.cursor() == 3);
	std::strcpy(field.text, "abcdefghijklmnopqrstuv");
	editor.reset(field);
	editor.adjustViewport(field);
	assert(editor.first() > 0);
	assert(editor.cursorX(field) + 2 <= field.x + field.width);
	dirty = false;
	assert(editor.key(field, KeyCode::Home, 0, event, dirty));
	editor.adjustViewport(field);
	assert(editor.first() == 0 && editor.cursorX(field) == field.x + 5);
}

void extendedTreeTests() {
	State state{};
	lexx::ui::Transport::Operations const operations{
	    &showLegacy, &poll, &close, &update, &setItems, &dialog, &sleep,
	    &showTree, &queryCapabilities, &showTreeV3, &patchTreeV3};
	lexx::ui::Transport transport{&state, operations};
	lexx::ui::UiCapabilities clientCaps{};
	assert(transport.query_capabilities(clientCaps) == 0);
	assert(clientCaps.protocolVersion == 3U && clientCaps.maxNodes == 32U);

	lexx::ui::Column root{100};
	lexx::ui::Button button{1, "Confirm"};
	lexx::ui::TreeWindowV3<24> window{"Big settings", transport};
	assert(window.add(root));
	assert(window.add(button, &root));
	assert(window.show() == 0);
	assert(state.shownV3 == 1 && state.lastV3Count == 2);
	assert(window.close() == 0);

	lexx::ui::TreeDocumentV3<24> client{};
	client.header.count = 20U;
	for (std::uint16_t i = 0; i < 20U; ++i) {
		client.nodes[i].control.id = static_cast<std::uint16_t>(i + 1U);
		client.nodes[i].control.kind = 2U;
		client.nodes[i].control.flags = 2U;
	}

	assert(transport.show_tree_v3(client) == 0);
	assert(state.shownV3 == 2 && state.lastV3Count == 20U);

	lexx::ui::TreePatchBatch<1> packet{};
	packet.header.count = 1U;
	packet.operations[0].kind = 2U;
	packet.operations[0].id = 1U;
	assert(transport.patch_tree_v3(packet) == 0);

	lexxos::ui::UiSession session{};
	auto const caps = lexxos::ui::UiService::capabilities(session);
	assert(caps.protocolVersion == 3U);
	assert(caps.maxNodes == lexxos::ui::treeNodeCapacity);
	assert(caps.maxControls == lexxos::ui::maxResolvedControls);
	assert((caps.flags & 3U) == 3U);

	lexxos::ui::TreeHeaderV3 header{};
	header.count = 20U;
	lexxos::ui::TreeNode nodes[20]{};
	for (std::uint16_t i = 0; i < 20U; ++i) {
		nodes[i].control.id = static_cast<std::uint16_t>(i + 1U);
		nodes[i].control.kind = 2U;
		nodes[i].control.flags = 2U;
	}

	lexxos::ui::ResolvedDocument result{};
	int scrollMax = 0, listIndex = -1;
	if constexpr (lexxos::ui::treeNodeCapacity >= 20U && lexxos::ui::maxResolvedControls >= 20U) {
		assert(lexxos::ui::tree::resolve(header, nodes, result, 320, 320, scrollMax, listIndex));
		assert(result.count == 20U && result.version == 3U && scrollMax > 0);
		lexxos::ui::GuestView view{};
		assert(view.configureTreeV3(header, nodes, 320, 320));
		assert(view.focusedId() == 1U);
		assert(view.focusControl(20U));
	} else {
		assert(!lexxos::ui::tree::resolve(header, nodes, result, 320, 320, scrollMax, listIndex));
	}

	header.version = 2U;
	assert(!lexxos::ui::tree::resolve(header, nodes, result, 320, 320, scrollMax, listIndex));
	header.version = 3U;
	nodes[1].control.id = 1U;
	assert(!lexxos::ui::tree::resolve(header, nodes, result, 320, 320, scrollMax, listIndex));
	static_assert(sizeof(lexx::ui::TreeHeaderV3) == sizeof(lexxos::ui::TreeHeaderV3));
	static_assert(sizeof(lexx::ui::TreeNode) == sizeof(lexxos::ui::TreeNode));
}

void multipleListStoresTests() {
	namespace layout = lexxos::ui::layout;
	namespace tree = lexxos::ui::tree;
	using lexxos::ui::Control;
	using lexxos::ui::GuestView;
	using lexxos::ui::ListItems;
	using lexxos::ui::maxListStores;
	using lexxos::ui::maxResolvedControls;
	using lexxos::ui::ResolvedDocument;
	using lexxos::ui::TreeHeaderV3;
	using lexxos::ui::TreeNode;
	using lexxos::ui::treeNodeCapacity;
	using lexxos::ui::TreePatchHeader;
	using lexxos::ui::TreePatchOperation;
	TreeHeaderV3 header{};
	header.count = 3U;
	TreeNode nodes[3]{};
	nodes[0].control.id = 100U;
	nodes[0].control.kind = 8U;
	nodes[0].control.flags = 2U;
	for (std::uint16_t i = 1; i < 3; ++i) {
		nodes[i].control.id = static_cast<std::uint16_t>(i + 10U);
		nodes[i].control.kind = i == 1U ? 10U : 11U;
		nodes[i].control.flags = 2U;
		nodes[i].parentId = 100U;
	}

	GuestView view{};
	assert(view.configureTreeV3(header, nodes, 320, 320));
	ListItems first{};
	first.id = 11U;
	first.count = 2U;
	std::strcpy(first.entries[0], "First");
	std::strcpy(first.entries[1], "Second");
	ListItems second{};
	second.id = 12U;
	second.count = 2U;
	std::strcpy(second.entries[0], "Alpha");
	std::strcpy(second.entries[1], "Beta");
	assert(view.setItems(first));
	assert(view.setItems(second));
	assert(view.itemsFor(11U).count == 2U);
	assert(std::strcmp(view.itemsFor(11U).entries[0], "First") == 0);
	assert(std::strcmp(view.itemsFor(12U).entries[0], "Alpha") == 0);
	assert(view.itemsFor(99U).count == 0U);
	ListItems invalid = first;
	invalid.id = 99U;
	assert(!view.setItems(invalid));

	GuestView updated{};
	assert(updated.configureTreeV3(header, nodes, 320, 320));
	updated.preserveControlState(view);
	assert(std::strcmp(updated.itemsFor(11U).entries[0], "First") == 0);
	assert(std::strcmp(updated.itemsFor(12U).entries[0], "Alpha") == 0);

	nodes[2].control.id = 13U;
	GuestView changed{};
	assert(changed.configureTreeV3(header, nodes, 320, 320));
	changed.preserveControlState(view);
	assert(changed.itemsFor(12U).count == 0U);
	assert(changed.itemsFor(13U).count == 0U);

	// Marker for active tab does not change the control's focus semantics.
	nodes[1].control.kind = 2U;
	nodes[1].control.flags = 2U | layout::tabActive;
	GuestView tab{};
	assert(tab.configureTreeV3(header, nodes, 320, 320));
	Control marker{};
	marker.id = 11U;
	marker.kind = 2U;
	marker.flags = 2U | layout::tabActive;
	assert(tab.update(marker));
	assert(tab.focusedId() == 11U);
	assert(!tab.focusControl(11U)); // Фокус уже установлен: изменения нет.

	TreeHeaderV3 excessive{};
	excessive.count = static_cast<std::uint16_t>(maxListStores + 1U);
	TreeNode tooMany[maxListStores + 1U]{};
	for (std::uint16_t i = 0; i < excessive.count; ++i) {
		tooMany[i].control.id = static_cast<std::uint16_t>(i + 1U);
		tooMany[i].control.kind = 10U;
		tooMany[i].control.flags = 2U;
	}

	GuestView limited{};
	assert(!limited.configureTreeV3(excessive, tooMany, 320, 320));
}

void treePatchTests() {
	namespace layout = lexxos::ui::layout;
	namespace tree = lexxos::ui::tree;
	using lexxos::ui::Control;
	using lexxos::ui::GuestView;
	using lexxos::ui::ListItems;
	using lexxos::ui::maxListStores;
	using lexxos::ui::maxResolvedControls;
	using lexxos::ui::ResolvedDocument;
	using lexxos::ui::TreeHeaderV3;
	using lexxos::ui::TreeNode;
	using lexxos::ui::treeNodeCapacity;
	using lexxos::ui::TreePatchHeader;
	using lexxos::ui::TreePatchOperation;
	TreeHeaderV3 source{};
	source.count = 5;
	TreeNode nodes[5]{};
	for (std::uint16_t i = 0; i < source.count; ++i) {
		nodes[i].control.id = static_cast<std::uint16_t>(i + 1U);
		nodes[i].control.kind = 2U;
		nodes[i].control.flags = 2U;
	}

	nodes[0].control.id = 100U;
	nodes[0].control.kind = 8U;
	nodes[1].parentId = 100U;
	nodes[2].parentId = 100U;
	nodes[3].control.id = 200U;
	nodes[3].control.kind = 8U;
	nodes[3].parentId = 100U;
	nodes[4].parentId = 200U;

	TreePatchHeader batch{3U, 1U};
	TreePatchOperation operations[3]{};
	operations[0].kind = 3U;
	operations[0].id = 2U;
	operations[0].parentId = 200U;
	operations[1].kind = 2U;
	operations[1].id = 3U;
	operations[2].kind = 1U;
	operations[2].id = 6U;
	operations[2].parentId = 100U;
	operations[2].node.control.id = 6U;
	operations[2].node.control.kind = 2U;
	operations[2].node.control.flags = 2U;
	operations[2].node.parentId = 100U;
	TreeNode output[16]{}, sorted[16]{};
	std::uint16_t count = 0;
	assert(tree::applyPatch(source, nodes, batch, operations, output, sorted, 16U, count));
	assert(count == 5U);
	bool moved = false;
	for (std::uint16_t i = 0; i < count; ++i) {
		if (output[i].control.id == 2U) {
			moved = output[i].parentId == 200U;
		}
	}

	assert(moved && nodes[1].parentId == 100U); // Original is unchanged.

	TreeHeaderV3 next = source;
	next.count = count;
	TreePatchHeader removal{1U, 1U};
	TreePatchOperation remove{};
	remove.kind = 2U;
	remove.id = 200U;
	TreeNode after[16]{}, temporary[16]{};
	std::uint16_t afterCount = 0;
	assert(tree::applyPatch(next, output, removal, &remove, after, temporary, 16U, afterCount));
	assert(afterCount == 2U); // The container and both descendants are gone.

	remove.kind = 3U;
	remove.id = 100U;
	remove.parentId = 2U;
	assert(!tree::applyPatch(next, output, removal, &remove, after, temporary, 16U, afterCount));
	remove.kind = 2U;
	remove.id = 999U;
	assert(!tree::applyPatch(next, output, removal, &remove, after, temporary, 16U, afterCount));
	remove.kind = 1U;
	remove.id = 6U; // Duplicate stable ID.
	remove.parentId = 100U;
	remove.node.control.id = 6U;
	remove.node.parentId = 100U;
	assert(!tree::applyPatch(next, output, removal, &remove, after, temporary, 16U, afterCount));
	remove.id = 7U;
	remove.node.control.id = 7U;
	remove.parentId = 999U;
	remove.node.parentId = 999U;
	assert(tree::applyPatch(next, output, removal, &remove, after, temporary, 16U, afterCount) == false);

	// One patch updates the selected tab while structurally replacing a page.
	TreePatchHeader mixed{3U, 1U};
	TreePatchOperation mixedOps[3]{};
	mixedOps[0].kind = 2U;
	mixedOps[0].id = 200U; // Remove old subtree.
	mixedOps[1].kind = 1U;
	mixedOps[1].id = 300U;
	mixedOps[1].parentId = 100U;
	mixedOps[1].node.parentId = 100U;
	mixedOps[1].node.control.id = 300U;
	mixedOps[1].node.control.kind = 8U;
	mixedOps[1].node.control.flags = 2U;
	mixedOps[2].kind = 4U;
	mixedOps[2].id = 2U;
	mixedOps[2].node.control.id = 2U;
	mixedOps[2].node.control.kind = 2U;
	mixedOps[2].node.control.flags = 2U | 8U;
	assert(tree::applyPatch(source, nodes, mixed, mixedOps, output, sorted, 16U, count));
	assert(count == 4U);
	bool active = false;
	for (std::uint16_t i = 0; i < count; ++i) {
		if (output[i].control.id == 2U) {
			active = (output[i].control.flags & 8U) != 0U;
		}
	}

	assert(active && nodes[1].control.flags == 2U);
	mixedOps[2].node.control.flags = 0x80U;
	assert(!tree::applyPatch(source, nodes, mixed, mixedOps, output, sorted, 16U, count));

	static_assert(sizeof(TreePatchHeader) == sizeof(lexx::ui::TreePatchHeader));
	static_assert(sizeof(TreePatchOperation) == sizeof(lexx::ui::TreePatchOperation));
}

} // namespace

int main() {
	multipleListStoresTests();
	batchedUpdateTests();
	treePatchTests();
	extendedTreeTests();
	layoutTests();
	horizontalFocusTests();
	spatialFocusTests();
	focusManagerAndCaretTests();
	textEditingTests();
}
