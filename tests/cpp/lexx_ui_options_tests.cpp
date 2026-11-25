#include <cassert>
#include <cstring>

#include <lexx/ui.h>

namespace {

struct State {
	int shown{0};
	int patches{0};
	int items{0};
	int updates{0};
	int batches{0};
	int lastPatchCount{0};
	bool rejectPatch{false};
	unsigned patchKindMask{0};
	lexx::ui::ListItems last{};
};

int show(void*, lexx::ui::Document const&) {
	return -1;
}

int poll(void*, lexx::ui::Event&) {
	return 0;
}

int close(void*) {
	return 0;
}

int update(void* data, lexx::ui::Control const&) {
	++static_cast<State*>(data)->updates;
	return 0;
}

int items(void* data, lexx::ui::ListItems const& value) {
	auto& state = *static_cast<State*>(data);
	++state.items;
	state.last = value;
	return 0;
}

int dialog(void*, lexx::ui::DialogDescriptor const&) {
	return 0;
}

void sleep(void*, lexx::uint32_t) {
}

int showTree(void*, lexx::ui::TreeDocument const&) {
	return -1;
}

int capabilities(void*, lexx::ui::UiCapabilities& data) {
	data = {3U, 16U, 16U, 3U};
	return 0;
}

int showV3(void* data, lexx::ui::TreeHeaderV3 const&, lexx::ui::TreeNode const*) {
	++static_cast<State*>(data)->shown;
	return 0;
}

int patchV3(void* data, lexx::ui::TreePatchHeader const& header,
    lexx::ui::TreePatchOperation const* operations) {
	if (header.version != 1U || header.count == 0U) {
		return -1;
	}

	auto& state = *static_cast<State*>(data);
	++state.patches;
	state.lastPatchCount = header.count;
	state.patchKindMask = 0U;
	for (lexx::uint16_t i = 0; i < header.count; ++i) {
		state.patchKindMask |= 1U << operations[i].kind;
	}

	return state.rejectPatch ? -3 : 0;
	return 0;
}

int updateBatch(void* data, lexx::ui::TreeUpdateHeader const& header, lexx::ui::Control const*) {
	assert(header.count > 0U);
	++static_cast<State*>(data)->batches;
	return 0;
}

void selectionTests() {
	State state{};
	lexx::ui::Transport::Operations const ops{
	    &show, &poll, &close, &update, &items, &dialog, &sleep,
	    &showTree, &capabilities, &showV3, &patchV3, &updateBatch};
	lexx::ui::Transport transport{&state, ops};
	lexx::ui::Column root{100};
	lexx::ui::ScrollList list{10};
	char const* const choices[]{
	    "a", "b", "c", "d", "e", "f", "g", "h", "i"};
	assert(list.set_items(choices, 9U) == 0);
	lexx::ui::TreeWindowV3<16> window{"Test", transport};
	assert(window.add(root));
	assert(window.add(list, &root));
	assert(window.show() == 0);
	assert(state.items == 1 && state.last.count == 4U);
	assert(std::strcmp(state.last.entries[0], "a") == 0);
	lexx::ui::Event event{};
	event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::Change);
	event.value = 0xFFFFFFFFU;
	list.dispatch(event);
	assert(list.selected_index() == 4U);
	assert(state.items == 2);
	assert(std::strcmp(state.last.entries[0], "b") == 0);
	assert(window.close() == 0);

	lexx::ui::Dropdown dropdown{11};
	assert(dropdown.set_items(choices, 9U) == 0);
	lexx::ui::Column secondRoot{101};
	lexx::ui::TreeWindowV3<16> dropdownWindow{"Dropdown", transport};
	assert(dropdownWindow.add(secondRoot));
	assert(dropdownWindow.add(dropdown, &secondRoot));
	assert(dropdownWindow.show() == 0);
	assert(state.last.count == 2U);
	event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::Change);
	event.value = 0x80000001U;
	dropdown.dispatch(event);
	assert(dropdown.selected_index() == 1U);
	assert(dropdownWindow.close() == 0);
}

void tabSelectionTests() {
	State state{};
	lexx::ui::Transport::Operations const ops{
	    &show, &poll, &close, &update, &items, &dialog, &sleep,
	    &showTree, &capabilities, &showV3, &patchV3, &updateBatch};
	lexx::ui::Transport transport{&state, ops};
	lexx::ui::TreeWindowV3<16> window{"Tabs", transport};
	lexx::ui::Column root{100}, bar{101}, pageA{102}, pageB{103};
	lexx::ui::Button first{1, "First"}, second{2, "Second"};

	struct Page {
		lexx::ui::TreeWindowV3<16>* window;
		lexx::ui::Column* root;
		lexx::ui::Column* page;
	};

	Page a{&window, &root, &pageA}, b{&window, &root, &pageB};
	auto page = [](void* context, bool visible) -> bool {
		auto& data = *static_cast<Page*>(context);
		return visible ? data.window->add(*data.page, data.root)
		               : data.window->remove(data.page->id()) == 0;
	};
	assert(window.add(root) && window.add(bar, &root));
	lexx::ui::Tabs<16, 2> tabs{window, bar};
	assert(tabs.add(first, page, &a));
	assert(tabs.add(second, page, &b));
	assert(tabs.select(0U));
	assert((first.descriptor().flags & 8U) != 0U);
	assert((second.descriptor().flags & 8U) == 0U);
	assert(window.show() == 0);
	assert(tabs.select(1U));
	assert(tabs.selected() == 1);
	assert((first.descriptor().flags & 8U) == 0U);
	assert((second.descriptor().flags & 8U) != 0U);
	assert(state.patches == 1);
	assert(state.lastPatchCount == 4); // Remove + insert + two tab flags.
	assert((state.patchKindMask & (1U << 4U)) != 0U);
	assert(state.batches == 0); // No second update_tree_batch operation.
	state.rejectPatch = true;
	assert(!tabs.select(0U));
	assert(tabs.selected() == 1);
	assert((second.descriptor().flags & 8U) != 0U);
	assert(pageA.id() == 102U && pageB.id() == 103U);
	state.rejectPatch = false;
	assert(tabs.select(0U));
	assert(state.patches == 3);
	assert(window.close() == 0);
}

void groupTests() {
	lexx::ui::Checkbox a{1, "One", true};
	lexx::ui::Checkbox b{2, "Two"};
	lexx::ui::CheckboxGroup<2> checks{100};
	assert(checks.add(a) && checks.add(b));
	assert(checks.selected_mask() == 1U);
	assert(b.set_checked(true) == 0);
	assert(checks.selected_mask() == 3U);

	lexx::ui::RadioButton first{3, "A", true};
	lexx::ui::RadioButton second{4, "B"};
	lexx::ui::RadioGroup<2> radio{101};
	assert(radio.add(first) && radio.add(second));
	assert(radio.selected_index() == 0);
	lexx::ui::Event event{};
	event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::Change);
	event.value = 1U;
	second.dispatch(event);
	assert(radio.selected_index() == 1);
	assert(!first.checked() && second.checked());

	State state{};
	lexx::ui::Transport::Operations const ops{
	    &show, &poll, &close, &update, &items, &dialog, &sleep,
	    &showTree, &capabilities, &showV3, &patchV3, &updateBatch};
	lexx::ui::Transport transport{&state, ops};
	lexx::ui::Column root{200};
	lexx::ui::TreeWindowV3<16> window{"Radio group", transport};
	assert(window.add(root) && window.show() == 0);
	assert(radio.mount(window, root));
	assert(radio.select(0U) == 0);
	assert(state.batches == 1);
	assert(radio.selected_index() == 0);
	assert(radio.unmount(window) == 0);
	assert(window.close() == 0);
}

} // namespace

int main() {
	tabSelectionTests();
	selectionTests();
	groupTests();
}
