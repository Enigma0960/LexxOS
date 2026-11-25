#include <lexxos/ui/ui_service.h>

#include <cstring>
#include <memory>
#include <new>
#include <utility>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/guest_graphics_mode.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/os/app_context.h>
#include <lexxos/ui/guest_ui_tree_patch.h>

namespace lexxos::ui {

namespace {

	constexpr int invalid = -1;
	constexpr int notFound = -2;
	constexpr int busy = -3;
	constexpr int denied = -4;
	constexpr int notReady = -5;
	constexpr int ioError = -7;
	constexpr int cancelledStatus = -9;

	bool isCancellationKey(devices::InputEvent const& input) {
		return input.character == 3U
		    || (input.key == devices::KeyCode::C
		        && (input.modifiers & devices::InputModifierCtrl) != 0U);
	}

} // namespace

bool UiService::canInteract(os::AppContext& context, UiSession const& session) {
	return session._owner != 0U && session._owner == context.processId() && session._view.active() && context.hasProgramFocus() && app::guest_graphics_mode::owns(session._owner);
}

int UiService::show(os::AppContext& context, UiSession& session, Document const& document) {
	if (document.count > session._budget.maxControls) {
		return invalid;
	}

	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		return notFound;
	}

	if (!display->isReady()) {
		return notReady;
	}

	auto* pending = new (std::nothrow) GuestView{};
	if (pending == nullptr) {
		return notReady;
	}

	int const status = pending->configure(document, display->width(), display->height())
	    ? present(context, session, *pending, *display)
	    : invalid;
	delete pending;
	if (status == 0) {
		session._treeNodes.reset();
		session._treeHeader = {};
	}

	return status;
}

int UiService::showTree(os::AppContext& context, UiSession& session, TreeDocument const& document) {
	if (document.count > session._budget.maxNodes) {
		return invalid;
	}

	std::uint16_t leaves = 0;
	for (std::uint16_t i = 0; i < document.count && i < maxTreeNodes; ++i) {
		leaves += ((document.nodes[i].control.kind >= 1U && document.nodes[i].control.kind <= 7U)
		              || (document.nodes[i].control.kind >= 10U && document.nodes[i].control.kind <= 12U))
		    ? 1U
		    : 0U;
	}

	if (leaves > session._budget.maxControls) {
		return invalid;
	}

	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		return notFound;
	}

	if (!display->isReady()) {
		return notReady;
	}

	auto* pending = new (std::nothrow) GuestView{};
	if (pending == nullptr) {
		return notReady;
	}

	int const status = pending->configureTree(document, display->width(), display->height())
	    ? present(context, session, *pending, *display)
	    : invalid;
	delete pending;
	if (status == 0) {
		session._treeNodes.reset();
		session._treeHeader = {};
	}

	return status;
}

int UiService::showTreeV3(os::AppContext& context, UiSession& session,
    TreeHeaderV3 const& document, TreeNode const* nodes) {
	if (document.version != 3U || document.count > session._budget.maxNodes || (document.count != 0U && nodes == nullptr)) {
		return invalid;
	}

	std::uint16_t leaves = 0;
	for (std::uint16_t i = 0; i < document.count; ++i) {
		leaves += ((nodes[i].control.kind >= 1U && nodes[i].control.kind <= 7U)
		              || (nodes[i].control.kind >= 10U && nodes[i].control.kind <= 12U))
		    ? 1U
		    : 0U;
	}

	if (leaves > session._budget.maxControls) {
		return invalid;
	}

	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		return notFound;
	}

	if (!display->isReady()) {
		return notReady;
	}

	std::unique_ptr<TreeNode[]> saved{new (std::nothrow) TreeNode[document.count]};
	auto* pending = new (std::nothrow) GuestView{};
	if (!saved || pending == nullptr) {
		delete pending;
		return notReady;
	}

	if (document.count != 0U) {
		std::memcpy(saved.get(), nodes, document.count * sizeof(TreeNode));
	}

	int const status = pending->configureTreeV3(document, saved.get(), display->width(), display->height())
	    ? present(context, session, *pending, *display)
	    : invalid;
	delete pending;
	if (status == 0) {
		session._treeHeader = document;
		session._treeNodes = std::move(saved);
	}

	return status;
}

int UiService::patchTreeV3(os::AppContext& context, UiSession& session,
    TreePatchHeader const& batch, TreePatchOperation const* operations) {
	if (!canInteract(context, session) || session._treeNodes == nullptr || session._view.dialogOpen()) {
		return busy;
	}

	if (batch.version != 1U || batch.count == 0U || batch.count > maxTreePatchOperations || operations == nullptr) {
		return invalid;
	}

	std::unique_ptr<TreeNode[]> candidate{new (std::nothrow) TreeNode[session._budget.maxNodes]};
	std::unique_ptr<TreeNode[]> sorted{new (std::nothrow) TreeNode[session._budget.maxNodes]};
	auto* pending = new (std::nothrow) GuestView{};
	if (!candidate || !sorted || pending == nullptr) {
		delete pending;
		return notReady;
	}

	std::uint16_t count = 0;
	bool const valid = tree::applyPatch(session._treeHeader, session._treeNodes.get(),
	    batch, operations, candidate.get(), sorted.get(), session._budget.maxNodes, count);
	if (!valid) {
		delete pending;
		return invalid;
	}

	TreeHeaderV3 header = session._treeHeader;
	header.count = count;
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr || !display->isReady()) {
		delete pending;
		return notReady;
	}

	int status = invalid;
	if (pending->configureTreeV3(header, candidate.get(), display->width(), display->height())) {
		pending->preserveControlState(session._view);
		status = present(context, session, *pending, *display);
	}

	delete pending;
	if (status == 0) {
		session._treeHeader = header;
		session._treeNodes = std::move(candidate);
	}

	return status;
}

int UiService::present(os::AppContext& context, UiSession& session,
    GuestView const& pending, devices::DisplayDevice& display) {
	if (!context.hasProgramFocus() || context.processId() == 0U || (session._owner != 0U && session._owner != context.processId())) {
		return busy;
	}

	bool const alreadyOwned = app::guest_graphics_mode::owns(context.processId());
	if (!app::guest_graphics_mode::acquire(context.processId())) {
		return busy;
	}

	bool rendered = false;
	bool acquiredLock = false;
	{
		auto lock = context.devices().lockDisplayDevice(display.name(),
		    devices::DeviceResourcePriority::Interactive, "lexx-system-ui");
		if (lock) {
			acquiredLock = true;
			app::console_mirror::setMode(app::ConsoleMirrorMode::Graphics);
			rendered = session._view.active() ? pending.renderChanges(display, session._view) : pending.render(display);
		}
	}

	if (!rendered) {
		if (!alreadyOwned) {
			app::guest_graphics_mode::release(context.processId());
			if (context.hasProgramFocus()) {
				app::console_mirror::setMode(app::ConsoleMirrorMode::Text);
				app::console_mirror::refresh();
			}
		}

		return acquiredLock ? ioError : busy;
	}

	session._view = pending;
	session._owner = context.processId();
	session._needsRedraw = false;
	return 0;
}

void UiService::redraw(os::AppContext& context, UiSession& session, GuestView const* previous) {
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr || !display->isReady()) {
		session._needsRedraw = true;
		return;
	}

	auto lock = context.devices().lockDisplayDevice(display->name(),
	    devices::DeviceResourcePriority::Interactive, "lexx-system-ui");
	session._needsRedraw = !lock || !(previous != nullptr && !session._needsRedraw ? session._view.renderChanges(*display, *previous) : session._view.render(*display));
}

int UiService::updateTreeBatch(os::AppContext& context, UiSession& session,
    TreeUpdateHeader const& header, Control const* controls) {
	if (!canInteract(context, session) || session._view.dialogOpen()) {
		return busy;
	}

	if (header.version != 1U || header.count == 0U || header.count > session._budget.maxControls || controls == nullptr) {
		return invalid;
	}

	std::unique_ptr<GuestView> candidate{new (std::nothrow) GuestView(session._view)};
	if (!candidate) {
		return notReady;
	}

	for (std::uint16_t i = 0; i < header.count; ++i) {
		for (std::uint16_t j = 0; j < i; ++j) {
			if (controls[i].id == controls[j].id) {
				return invalid;
			}
		}

		if (!candidate->update(controls[i])) {
			return invalid;
		}
	}

	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr || !display->isReady()) {
		return notReady;
	}

	return present(context, session, *candidate, *display);
}

int UiService::update(os::AppContext& context, UiSession& session, Control const& control) {
	if (!canInteract(context, session)) {
		return busy;
	}

	std::unique_ptr<GuestView> previous{new (std::nothrow) GuestView(session._view)};
	if (!session._view.update(control)) {
		return invalid;
	}

	redraw(context, session, previous.get());
	return 0;
}

int UiService::setItems(os::AppContext& context, UiSession& session, ListItems const& items) {
	if (!canInteract(context, session)) {
		return busy;
	}

	std::unique_ptr<GuestView> previous{new (std::nothrow) GuestView(session._view)};
	if (!session._view.setItems(items)) {
		return invalid;
	}

	redraw(context, session, previous.get());
	return 0;
}

int UiService::showDialog(os::AppContext& context, UiSession& session, DialogDescriptor const& dialog) {
	if (!canInteract(context, session)) {
		return busy;
	}

	if (!session._view.showDialog(dialog)) {
		return invalid;
	}

	redraw(context, session);
	return 0;
}

int UiService::poll(os::AppContext& context, UiSession& session,
    app::program_input::EventCursor& cursor, Event& event, bool& cancelled) {
	event = {};
	cancelled = false;
	if (!canInteract(context, session)) {
		return busy;
	}

	if (context.cancellationRequested()) {
		cancelled = true;
		return cancelledStatus;
	}

	std::unique_ptr<GuestView> previous{};
	bool dirty = false;
	devices::InputEvent input{};
	if (context.programInput().pollEvent(cursor, input)) {
		if (isCancellationKey(input)) {
			context.requestCancellation();
			cancelled = true;
			return cancelledStatus;
		}

		if (input.type == devices::InputEventType::KeyDown) {
			previous.reset(new (std::nothrow) GuestView(session._view));
			(void) session._view.key(input.key, static_cast<char>(input.character), event, dirty);
		}
	} else {
		char character = 0;
		if (context.consoleInput().readChar(context.platform(), character)) {
			if (static_cast<std::uint8_t>(character) == 3U) {
				context.requestCancellation();
				cancelled = true;
				return cancelledStatus;
			}

			previous.reset(new (std::nothrow) GuestView(session._view));
			(void) session._view.key(devices::KeyCode::Unknown, character, event, dirty);
		}
	}

	if (dirty || session._needsRedraw) {
		redraw(context, session, previous.get());
	}

	return event.type != 0U ? 1 : 0;
}

void UiService::close(os::AppContext& context, UiSession& session) {
	std::uint32_t const owner = session._owner;
	session._view.reset();
	session._treeNodes.reset();
	session._treeHeader = {};
	session._needsRedraw = false;
	session._owner = 0;
	if (owner == 0U) {
		return;
	}

	app::guest_graphics_mode::release(owner);
	if (context.hasProgramFocus() && app::guest_graphics_mode::ownerProcessId.load() == 0U) {
		app::console_mirror::setMode(app::ConsoleMirrorMode::Text);
		app::console_mirror::refresh();
	}
}

} // namespace lexxos::ui
