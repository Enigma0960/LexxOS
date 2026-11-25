#include <cassert>
#include <cstring>

#include <lexx/ui.h>
#include <lexx/ui/transport.h>
#include <ui-demo/shared_ui_demo.h>

namespace {

struct State {
	int shows{0};
	int updates{0};
	int sets{0};
	int dialogs{0};
	int closes{0};
	int polls{0};
	lexx::ui::Document last{};
};

int show(void* context, lexx::ui::Document const& document) {
	auto& state = *static_cast<State*>(context);
	state.last = document;
	++state.shows;
	return 0;
}

int poll(void* context, lexx::ui::Event& event) {
	auto& state = *static_cast<State*>(context);
	event = {};
	++state.polls;
	if (state.polls == 1) {
		event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::Activate);
		event.id = 7; // Save: callback opens a system dialog.
	} else if (state.polls == 2) {
		event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::DialogResult);
		event.id = 1;
		event.value = 1;
	} else {
		event.type = static_cast<lexx::uint16_t>(lexx::ui::EventType::Close);
	}

	return 1;
}

int close(void* context) {
	++static_cast<State*>(context)->closes;
	return 0;
}

int update(void* context, lexx::ui::Control const&) {
	++static_cast<State*>(context)->updates;
	return 0;
}

int items(void* context, lexx::ui::ListItems const& list) {
	assert(list.id == 4 && list.count == 3);
	++static_cast<State*>(context)->sets;
	return 0;
}

int dialog(void* context, lexx::ui::DialogDescriptor const& data) {
	assert(data.id == 1 && data.kind == 1 && data.buttons == 1);
	++static_cast<State*>(context)->dialogs;
	return 0;
}

void sleep(void*, lexx::uint32_t) {
}

} // namespace

int main() {
	State state{};
	lexx::ui::Transport::Operations const operations{&show, &poll, &close, &update, &items, &dialog, &sleep};
	lexx::ui::Transport transport{&state, operations};
	lexxos::ui_demo::Demo demo{transport};
	assert(demo.run() == 0);
	assert(state.shows == 1 && state.closes == 1);
	assert(state.dialogs == 1 && state.sets == 1 && state.updates == 0);
	assert(state.last.count == 8);
	assert(std::strcmp(state.last.title, "LexxUI Settings") == 0);
	assert(state.last.controls[7].id == 8);
	assert((state.last.controls[7].flags & lexx::ui::flag_same_row) != 0);
}
