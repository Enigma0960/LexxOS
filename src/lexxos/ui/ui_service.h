#pragma once

#include <cstdint>

#include <lexxos/app/program_input.h>
#include <lexxos/ui/guest_ui.h>
#include <lexxos/ui/ui_session.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::ui {

// Each UI client owns its session. A session never outlives its process;
// no VM guest pointers, renderer pointers, or callbacks are stored in the OS.

// Common OS implementation for the native transport and the RV32I syscall
// adapter. Both transports submit copies of the same ABI records.
class UiService final {
public:
	static int show(os::AppContext& context, UiSession& session, Document const& document);
	static int showTree(os::AppContext& context, UiSession& session, TreeDocument const& document);
	static int showTreeV3(os::AppContext& context, UiSession& session, TreeHeaderV3 const& document, TreeNode const* nodes);

	static UiCapabilities capabilities(UiSession const& session) {
		return {3U, session._budget.maxNodes, session._budget.maxControls, 7U};
	}

	static int updateTreeBatch(os::AppContext& context, UiSession& session,
	    TreeUpdateHeader const& header, Control const* controls);
	static int patchTreeV3(os::AppContext& context, UiSession& session,
	    TreePatchHeader const& batch, TreePatchOperation const* operations);
	static int update(os::AppContext& context, UiSession& session, Control const& control);
	static int setItems(os::AppContext& context, UiSession& session, ListItems const& items);
	static int showDialog(os::AppContext& context, UiSession& session, DialogDescriptor const& dialog);
	static int poll(os::AppContext& context, UiSession& session,
	    app::program_input::EventCursor& cursor, Event& event, bool& cancelled);
	static void close(os::AppContext& context, UiSession& session);

private:
	static int present(os::AppContext& context, UiSession& session,
	    GuestView const& pending, devices::DisplayDevice& display);
	static bool canInteract(os::AppContext& context, UiSession const& session);
	static void redraw(os::AppContext& context, UiSession& session, GuestView const* previous = nullptr);
};

} // namespace lexxos::ui
