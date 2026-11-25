#pragma once

#include <lexx/ui/core.h>
#include <lexx/ui/tree_document.h>

namespace lexx::ui {

// One object API, two backends. The transport does not own a widget and the
// guest never passes C++ pointers/callbacks across the kernel boundary.
class Transport {
public:
	struct Operations {
		int (*show)(void*, Document const&);
		int (*poll)(void*, Event&);
		int (*close)(void*);
		int (*update)(void*, Control const&);
		int (*setItems)(void*, ListItems const&);
		int (*showDialog)(void*, DialogDescriptor const&);
		void (*sleepMs)(void*, uint32_t);
		int (*showTree)(void*, TreeDocument const&){nullptr};
		int (*capabilities)(void*, UiCapabilities&){nullptr};
		int (*showTreeV3)(void*, TreeHeaderV3 const&, TreeNode const*){nullptr};
		int (*patchTreeV3)(void*, TreePatchHeader const&, TreePatchOperation const*){nullptr};
		int (*updateTreeBatch)(void*, TreeUpdateHeader const&, Control const*){nullptr};
	};

	constexpr Transport(void* context, Operations const& operations)
	    : _context(context)
	    , _operations(&operations) {}

	[[nodiscard]] int show(Document const& value) const { return _operations->show(_context, value); }

	[[nodiscard]] int poll(Event& value) const { return _operations->poll(_context, value); }

	[[nodiscard]] int close() const { return _operations->close(_context); }

	[[nodiscard]] int update(Control const& value) const { return _operations->update(_context, value); }

	[[nodiscard]] int set_items(ListItems const& value) const { return _operations->setItems(_context, value); }

	[[nodiscard]] int show_dialog(DialogDescriptor const& value) const { return _operations->showDialog(_context, value); }

	void sleep_ms(uint32_t ms) const { _operations->sleepMs(_context, ms); }

	[[nodiscard]] int show_tree(TreeDocument const& value) const {
		return _operations->showTree != nullptr ? _operations->showTree(_context, value) : -1;
	}

	[[nodiscard]] int query_capabilities(UiCapabilities& value) const {
		return _operations->capabilities != nullptr ? _operations->capabilities(_context, value) : -1;
	}

	template <uint16_t Capacity>
	[[nodiscard]] int show_tree_v3(TreeDocumentV3<Capacity> const& value) const {
		if (_operations->showTreeV3 == nullptr || value.header.version != tree_protocol_v3 || value.header.count > Capacity) {
			return -1;
		}
		return _operations->showTreeV3(_context, value.header, value.nodes);
	}

	template <uint16_t Capacity>
	[[nodiscard]] int patch_tree_v3(TreePatchBatch<Capacity> const& value) const {
		if (_operations->patchTreeV3 == nullptr || value.header.version != 1U || value.header.count == 0U || value.header.count > Capacity) {
			return -1;
		}
		return _operations->patchTreeV3(_context, value.header, value.operations);
	}

	template <uint16_t Capacity>
	[[nodiscard]] int update_tree_batch(TreeUpdateBatch<Capacity> const& value) const {
		if (_operations->updateTreeBatch == nullptr || value.header.version != 1U || value.header.count == 0U || value.header.count > Capacity) {
			return -1;
		}
		return _operations->updateTreeBatch(_context, value.header, value.controls);
	}

private:
	void* _context;
	Operations const* _operations;
};

#if !defined(LEXXOS_UI_NATIVE)
namespace detail {

	inline int guestShow(void*, Document const& d) {
		return ui::show(d);
	}

	inline int guestPoll(void*, Event& e) {
		return ui::poll(e);
	}

	inline int guestClose(void*) {
		return ui::close();
	}

	inline int guestUpdate(void*, Control const& c) {
		return ui::update_control(c);
	}

	inline int guestSetItems(void*, ListItems const& i) {
		return ui::set_items(i);
	}

	inline int guestDialog(void*, DialogDescriptor const& d) {
		return ui::show_dialog(d);
	}

	inline void guestSleep(void*, uint32_t ms) {
		lexx::sleep_ms(ms);
	}

	inline int guestShowTree(void*, TreeDocument const& document) {
		return static_cast<int>(::lexx::detail::ecall3(syscall_graphics, operation_show_tree,
		    reinterpret_cast<uint32_t>(&document), 0U));
	}

	inline int guestCapabilities(void*, UiCapabilities& result) {
		return ui::capabilities(result);
	}

	inline int guestShowTreeV3(void*, TreeHeaderV3 const& header, TreeNode const* nodes) {
		if (nodes != reinterpret_cast<TreeNode const*>(reinterpret_cast<uint8_t const*>(&header) + sizeof(TreeHeaderV3))) {
			return -1;
		}

		return static_cast<int>(::lexx::detail::ecall3(syscall_graphics, operation_show_tree_v3,
		    reinterpret_cast<uint32_t>(&header),
		    static_cast<uint32_t>(sizeof(TreeHeaderV3) + header.count * sizeof(TreeNode))));
	}

	inline int guestPatchTreeV3(void*, TreePatchHeader const& header, TreePatchOperation const* operations) {
		if (operations != reinterpret_cast<TreePatchOperation const*>(reinterpret_cast<uint8_t const*>(&header) + sizeof(TreePatchHeader))) {
			return -1;
		}

		return static_cast<int>(::lexx::detail::ecall3(syscall_graphics, operation_patch_tree_v3,
		    reinterpret_cast<uint32_t>(&header),
		    static_cast<uint32_t>(sizeof(TreePatchHeader) + header.count * sizeof(TreePatchOperation))));
	}

	inline int guestUpdateTreeBatch(void*, TreeUpdateHeader const& header, Control const* controls) {
		if (controls != reinterpret_cast<Control const*>(reinterpret_cast<uint8_t const*>(&header) + sizeof(TreeUpdateHeader))) {
			return -1;
		}

		return static_cast<int>(::lexx::detail::ecall3(syscall_graphics, operation_update_tree_batch,
		    reinterpret_cast<uint32_t>(&header),
		    static_cast<uint32_t>(sizeof(TreeUpdateHeader) + header.count * sizeof(Control))));
	}

	inline constexpr Transport::Operations guestOperations{
	    &guestShow, &guestPoll, &guestClose, &guestUpdate,
	    &guestSetItems, &guestDialog, &guestSleep, &guestShowTree,
	    &guestCapabilities, &guestShowTreeV3, &guestPatchTreeV3, &guestUpdateTreeBatch};

} // namespace lexx::ui::detail

inline constexpr Transport guestTransport{nullptr, detail::guestOperations};
#endif

} // namespace lexx::ui
