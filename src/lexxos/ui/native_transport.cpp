#include <lexxos/ui/native_transport.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::ui {

namespace {

	template <typename Native, typename Wire>
	Native copyRecord(Wire const& source) {
		static_assert(sizeof(Native) == sizeof(Wire));
		static_assert(alignof(Native) == alignof(Wire));
		static_assert(std::is_trivially_copyable_v<Native> && std::is_trivially_copyable_v<Wire>);
		Native result{};
		std::memcpy(static_cast<void*>(&result), static_cast<void const*>(&source), sizeof(result));
		return result;
	}

	template <typename Native, typename Wire>
	void copyRecords(Native* destination, Wire const* source, std::size_t count) {
		static_assert(sizeof(Native) == sizeof(Wire));
		static_assert(alignof(Native) == alignof(Wire));
		static_assert(std::is_trivially_copyable_v<Native> && std::is_trivially_copyable_v<Wire>);

		std::memcpy(static_cast<void*>(destination),
		    static_cast<void const*>(source), count * sizeof(Native));
	}

} // namespace

lexx::ui::Transport::Operations const NativeTransport::operations{
    &NativeTransport::show,
    &NativeTransport::poll,
    &NativeTransport::close,
    &NativeTransport::update,
    &NativeTransport::setItems,
    &NativeTransport::showDialog,
    &NativeTransport::sleepMs,
    &NativeTransport::showTree,
    &NativeTransport::capabilities,
    &NativeTransport::showTreeV3,
    &NativeTransport::patchTreeV3,
    &NativeTransport::updateTreeBatch,
};

NativeTransport::NativeTransport(os::AppContext& context)
    : _context(context) {
	_context.programInput().reset(_cursor);
}

NativeTransport::~NativeTransport() {
	UiService::close(_context, _session);
}

int NativeTransport::show(void* self, lexx::ui::Document const& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	transport._context.programInput().reset(transport._cursor);
	return UiService::show(transport._context, transport._session, copyRecord<Document>(wire));
}

int NativeTransport::showTree(void* self, lexx::ui::TreeDocument const& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	transport._context.programInput().reset(transport._cursor);
	return UiService::showTree(transport._context, transport._session, copyRecord<TreeDocument>(wire));
}

int NativeTransport::capabilities(void* self, lexx::ui::UiCapabilities& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	wire = copyRecord<lexx::ui::UiCapabilities>(UiService::capabilities(transport._session));
	return 0;
}

int NativeTransport::showTreeV3(void* self, lexx::ui::TreeHeaderV3 const& wire,
    lexx::ui::TreeNode const* nodes) {
	auto& transport = *static_cast<NativeTransport*>(self);
	auto const header = copyRecord<TreeHeaderV3>(wire);
	if (header.count > transport._session.budget().maxNodes || nodes == nullptr) {
		return -1;
	}

	auto* const copied = new (std::nothrow) TreeNode[header.count];
	if (copied == nullptr) {
		return -5;
	}

	copyRecords(copied, nodes, header.count);
	transport._context.programInput().reset(transport._cursor);
	int const status = UiService::showTreeV3(transport._context, transport._session, header, copied);
	delete[] copied;
	return status;
}

int NativeTransport::patchTreeV3(void* self, lexx::ui::TreePatchHeader const& wire,
    lexx::ui::TreePatchOperation const* operations) {
	auto& transport = *static_cast<NativeTransport*>(self);
	auto const header = copyRecord<TreePatchHeader>(wire);
	if (header.version != 1U || header.count == 0U || header.count > maxTreePatchOperations || operations == nullptr) {
		return -1;
	}

	auto* copied = new (std::nothrow) TreePatchOperation[header.count];
	if (copied == nullptr) {
		return -5;
	}

	copyRecords(copied, operations, header.count);
	int const status = UiService::patchTreeV3(transport._context, transport._session, header, copied);
	delete[] copied;
	return status;
}

int NativeTransport::updateTreeBatch(void* self, lexx::ui::TreeUpdateHeader const& wire,
    lexx::ui::Control const* controls) {
	auto& transport = *static_cast<NativeTransport*>(self);
	auto const header = copyRecord<TreeUpdateHeader>(wire);
	if (header.version != 1U || header.count == 0U || header.count > transport._session.budget().maxControls || controls == nullptr) {
		return -1;
	}

	static_assert(sizeof(Control) == sizeof(lexx::ui::Control));
	std::unique_ptr<Control[]> copied{new (std::nothrow) Control[header.count]};
	if (!copied) {
		return -5;
	}

	copyRecords(copied.get(), controls, header.count);
	return UiService::updateTreeBatch(transport._context, transport._session, header, copied.get());
}

int NativeTransport::poll(void* self, lexx::ui::Event& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	Event event{};
	bool cancelled = false;
	int const status = UiService::poll(transport._context, transport._session,
	    transport._cursor, event, cancelled);
	wire = copyRecord<lexx::ui::Event>(event);
	return status;
}

int NativeTransport::close(void* self) {
	auto& transport = *static_cast<NativeTransport*>(self);
	UiService::close(transport._context, transport._session);
	return 0;
}

int NativeTransport::update(void* self, lexx::ui::Control const& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	return UiService::update(transport._context, transport._session, copyRecord<Control>(wire));
}

int NativeTransport::setItems(void* self, lexx::ui::ListItems const& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	return UiService::setItems(transport._context, transport._session, copyRecord<ListItems>(wire));
}

int NativeTransport::showDialog(void* self, lexx::ui::DialogDescriptor const& wire) {
	auto& transport = *static_cast<NativeTransport*>(self);
	return UiService::showDialog(transport._context, transport._session,
	    copyRecord<DialogDescriptor>(wire));
}

void NativeTransport::sleepMs(void*, lexx::uint32_t ms) {
	lexxos::kernel::sleepMs(ms);
}

} // namespace lexxos::ui
