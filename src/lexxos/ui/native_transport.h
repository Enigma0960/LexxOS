#pragma once

#include <lexx/ui/transport.h>
#include <lexxos/app/program_input.h>
#include <lexxos/ui/ui_service.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::ui {

// Native applications pass this transport explicitly to Window/MessageBox.
// Context and session are process-local; no global 'current application'.
class NativeTransport final {
public:
	explicit NativeTransport(os::AppContext& context);
	~NativeTransport();

	NativeTransport(NativeTransport const&) = delete;
	NativeTransport& operator=(NativeTransport const&) = delete;

	[[nodiscard]] lexx::ui::Transport const& transport() const { return _transport; }

private:
	static int show(void*, lexx::ui::Document const&);
	static int showTree(void*, lexx::ui::TreeDocument const&);
	static int capabilities(void*, lexx::ui::UiCapabilities&);
	static int showTreeV3(void*, lexx::ui::TreeHeaderV3 const&, lexx::ui::TreeNode const*);
	static int patchTreeV3(void*, lexx::ui::TreePatchHeader const&, lexx::ui::TreePatchOperation const*);
	static int updateTreeBatch(void*, lexx::ui::TreeUpdateHeader const&, lexx::ui::Control const*);
	static int poll(void*, lexx::ui::Event&);
	static int close(void*);
	static int update(void*, lexx::ui::Control const&);
	static int setItems(void*, lexx::ui::ListItems const&);
	static int showDialog(void*, lexx::ui::DialogDescriptor const&);
	static void sleepMs(void*, lexx::uint32_t);

	static lexx::ui::Transport::Operations const operations;

	os::AppContext& _context;
	UiSession _session{};
	app::program_input::EventCursor _cursor{};
	lexx::ui::Transport _transport{this, operations};
};

} // namespace lexxos::ui
