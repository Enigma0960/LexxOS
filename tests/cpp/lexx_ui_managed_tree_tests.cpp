#include <cassert>

#include <lexx/ui/button.h>
#include <lexx/ui/tree_window_v3.h>

namespace {

struct State {
	int shown{0};
	int patched{0};
	int closed{0};
	bool reject{false};
	lexx::ui::TreeDocumentV3<12> last{};
};

int showLegacy(void*, lexx::ui::Document const&) {
	return -1;
}

int poll(void*, lexx::ui::Event&) {
	return 0;
}

int close(void* value) {
	++static_cast<State*>(value)->closed;
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

int showTree(void*, lexx::ui::TreeDocument const&) {
	return -1;
}

int capabilities(void*, lexx::ui::UiCapabilities& result) {
	result = {3U, 12U, 12U, 3U};
	return 0;
}

int showV3(void* value, lexx::ui::TreeHeaderV3 const& header, lexx::ui::TreeNode const* nodes) {
	auto& state = *static_cast<State*>(value);
	state.last.header = header;
	for (lexx::uint16_t i = 0; i < header.count; ++i) {
		state.last.nodes[i] = nodes[i];
	}

	++state.shown;
	return 0;
}

int patch(void* value, lexx::ui::TreePatchHeader const& header,
    lexx::ui::TreePatchOperation const* operations) {
	auto& state = *static_cast<State*>(value);
	assert(header.count == 1 && operations[0].id != 0);
	++state.patched;
	return state.reject ? -3 : 0;
}

} // namespace

int main() {
	State state{};
	lexx::ui::Transport::Operations const ops{
	    &showLegacy, &poll, &close, &update, &setItems, &dialog, &sleep,
	    &showTree, &capabilities, &showV3, &patch};
	lexx::ui::Transport transport{&state, ops};
	lexx::ui::Column root{100};
	lexx::ui::Column nested{101};
	lexx::ui::Button first{1, "First"};
	lexx::ui::Button second{2, "Second"};
	lexx::ui::Button third{3, "Third"};
	lexx::ui::TreeWindowV3<12> window{"Managed", transport};
	assert(window.add(root));
	assert(window.add(nested, &root));
	assert(window.add(first, &nested));
	assert(window.show() == 0 && first.attached());

	state.reject = true;
	assert(!window.add(second, &nested));
	assert(!second.attached());
	assert(window.reparent(first.id(), &root) == -3);
	assert(window.remove(nested.id()) == -3);
	assert(first.attached());

	state.reject = false;
	assert(window.add(second, &nested) && second.attached());
	assert(window.add(third, &root) && third.attached());
	assert(!window.add(first, &root)); // Existing node/attached object.
	assert(window.reparent(first.id(), &root) == 0);
	assert(window.reparent(root.id(), &nested) == -1); // Cycle.
	assert(window.remove(nested.id()) == 0);
	assert(!second.attached() && first.attached() && third.attached());
	assert(window.remove(999U) == -1);

	assert(window.close() == 0 && !first.attached() && !third.attached());
	assert(window.show() == 0);
	assert(state.last.header.count == 3);
	assert(state.last.nodes[0].control.id == 100U);
	assert(state.last.nodes[1].control.id == 1U);
	assert(state.last.nodes[1].parentId == 100U);
	assert(state.last.nodes[2].control.id == 3U);
	assert(window.close() == 0);
}
