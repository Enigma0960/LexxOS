#pragma once

#include <lexx/ui/core.h>

namespace lexx::ui {

// UI v2 is a separate wire record; the v1 Document remains unchanged.
inline constexpr uint16_t tree_protocol_version = 2U;
inline constexpr uint16_t max_tree_nodes = 16U;
inline constexpr uint8_t kind_column = 8U;
inline constexpr uint8_t kind_row = 9U;
inline constexpr uint32_t operation_show_tree = 12U;
inline constexpr uint32_t operation_capabilities = 13U;
inline constexpr uint32_t operation_show_tree_v3 = 14U;
inline constexpr uint32_t operation_patch_tree_v3 = 15U;
inline constexpr uint32_t operation_update_tree_batch = 16U;
inline constexpr uint16_t max_tree_patch_operations = 16U;
inline constexpr uint16_t capability_counted_tree = 1U;
inline constexpr uint16_t capability_atomic_tree_patch = 2U;
inline constexpr uint16_t capability_atomic_patch_flags = 4U;
inline constexpr uint16_t tree_protocol_v3 = 3U;

struct TreeNode {
	Control control{};
	uint16_t parentId{0};
	uint16_t reserved{0};
};

struct TreeHeaderV3 {
	char title[32]{};
	uint16_t count{0};
	uint16_t version{tree_protocol_v3};
};

struct UiCapabilities {
	uint16_t protocolVersion{0};
	uint16_t maxNodes{0};
	uint16_t maxControls{0};
	uint16_t flags{0};
};

struct TreeUpdateHeader {
	uint16_t count{0};
	uint16_t version{1U};
};

template <uint16_t Capacity>
struct TreeUpdateBatch {
	static_assert(Capacity > 0 && Capacity <= 256);
	TreeUpdateHeader header{};
	Control controls[Capacity]{};
};

struct TreePatchHeader {
	uint16_t count{0};
	uint16_t version{1U};
};

struct TreePatchOperation {
	uint16_t kind{0};
	uint16_t id{0};
	uint16_t parentId{0};
	uint16_t reserved{0};
	TreeNode node{};
};

template <uint16_t Capacity>
struct TreePatchBatch {
	static_assert(Capacity > 0 && Capacity <= max_tree_patch_operations);
	TreePatchHeader header{};
	TreePatchOperation operations[Capacity]{};
};

// Contiguous counted document. Capacity belongs to the application; only
// 36 + count*52 bytes are copied across the guest boundary.
template <uint16_t Capacity>
struct TreeDocumentV3 {
	static_assert(Capacity > 0 && Capacity <= 256);
	TreeHeaderV3 header{};
	TreeNode nodes[Capacity]{};
};

struct TreeDocument {
	char title[32]{};
	uint16_t count{0};
	uint16_t version{tree_protocol_version};
	TreeNode nodes[max_tree_nodes]{};
};

static_assert(sizeof(TreeNode) == 52);
static_assert(sizeof(TreeDocument) == 868);
static_assert(sizeof(TreeHeaderV3) == 36);
static_assert(sizeof(UiCapabilities) == 8);
static_assert(sizeof(TreePatchHeader) == 4);
static_assert(sizeof(TreeUpdateHeader) == 4);
static_assert(sizeof(TreePatchOperation) == 60);

#if !defined(LEXXOS_UI_NATIVE)
inline int capabilities(UiCapabilities& output) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_capabilities,
	    reinterpret_cast<uint32_t>(&output), 0U));
}

template <uint16_t Capacity>
inline int show_tree_v3(TreeDocumentV3<Capacity> const& document) {
	if (document.header.version != tree_protocol_v3 || document.header.count > Capacity) {
		return -1;
	}

	return static_cast<int>(detail::ecall3(syscall_graphics, operation_show_tree_v3,
	    reinterpret_cast<uint32_t>(&document),
	    static_cast<uint32_t>(sizeof(TreeHeaderV3) + document.header.count * sizeof(TreeNode))));
}

template <uint16_t Capacity>
inline int update_tree_batch(TreeUpdateBatch<Capacity> const& batch) {
	if (batch.header.version != 1U || batch.header.count == 0U || batch.header.count > Capacity) {
		return -1;
	}

	return static_cast<int>(detail::ecall3(syscall_graphics, operation_update_tree_batch,
	    reinterpret_cast<uint32_t>(&batch),
	    static_cast<uint32_t>(sizeof(TreeUpdateHeader) + batch.header.count * sizeof(Control))));
}

template <uint16_t Capacity>
inline int patch_tree_v3(TreePatchBatch<Capacity> const& batch) {
	if (batch.header.version != 1U || batch.header.count == 0U || batch.header.count > Capacity) {
		return -1;
	}

	return static_cast<int>(detail::ecall3(syscall_graphics, operation_patch_tree_v3,
	    reinterpret_cast<uint32_t>(&batch),
	    static_cast<uint32_t>(sizeof(TreePatchHeader) + batch.header.count * sizeof(TreePatchOperation))));
}

#endif

} // namespace lexx::ui
