#pragma once

#include <cstdint>

#include <lexxos/runtime/selected_runtime_profile.h>

namespace lexxos::ui {

// Fixed guest ABI v1. No host pointers, heap allocations or callbacks.
struct Control {
	std::uint16_t id{0};
	std::uint8_t kind{0};
	std::uint8_t flags{0};
	std::int16_t x{0};
	std::int16_t y{0};
	std::int16_t width{0};
	std::int16_t height{0};
	std::uint32_t value{0};
	char text[32]{};
};

struct ListItems {
	std::uint16_t id{0};
	std::uint16_t count{0};
	char entries[8][32]{};
};

struct Document {
	char title[32]{};
	std::uint16_t count{0};
	std::uint16_t version{1};
	Control controls[8]{};
};

struct Event {
	std::uint16_t type{0};
	std::uint16_t id{0};
	std::uint32_t value{0};
	char text[32]{};
};

// Dialogs live outside the eight-control Document and preserve its binary ABI.
struct DialogDescriptor {
	char title[32]{};
	char message[96]{};
	std::uint16_t id{0};
	std::uint8_t kind{0}; // 1 information, 2 warning, 3 error, 4 question
	std::uint8_t buttons{0}; // 1 OK, 2 OK + Cancel
};

static_assert(sizeof(DialogDescriptor) == 132);
static_assert(sizeof(Control) == 48);
static_assert(sizeof(Document) == 420);
static_assert(sizeof(Event) == 40);
static_assert(sizeof(ListItems) == 260);

// Protocol v2 never changes the binary layout of the legacy v1 Document.
inline constexpr std::uint16_t maxTreeNodes = 16U;

// Private pool capacity follows the selected chip/platform/board profile.
// All firmware translation units must use the same runtime profile selection.
inline constexpr std::size_t resolvedControlCapacity =
    runtime::standardRuntimeProfile.ui.maxResolvedControls;
inline constexpr std::size_t treeNodeCapacity = runtime::standardRuntimeProfile.ui.maxTreeNodes;
static_assert(resolvedControlCapacity >= 8U && resolvedControlCapacity <= treeNodeCapacity);
static_assert(treeNodeCapacity >= maxTreeNodes && treeNodeCapacity <= 256U);
inline constexpr std::uint16_t maxResolvedControls =
    static_cast<std::uint16_t>(resolvedControlCapacity);
inline constexpr std::size_t maxListStores =
    runtime::standardRuntimeProfile.ui.maxListStores;
static_assert(maxListStores > 0U && maxListStores <= resolvedControlCapacity);

struct UiBudget {
	std::uint16_t maxNodes{static_cast<std::uint16_t>(treeNodeCapacity)};
	std::uint16_t maxControls{maxResolvedControls};

	[[nodiscard]] constexpr bool valid() const {
		return maxNodes > 0U && maxNodes <= treeNodeCapacity && maxControls > 0U && maxControls <= maxResolvedControls;
	}
};

struct TreeNode {
	Control control{};
	std::uint16_t parentId{0};
	std::uint16_t reserved{0};
};

// v3 carries the same 52-byte node records, but no fixed-size tree array.
// The node sequence immediately follows the 36-byte versioned header.
struct TreeHeaderV3 {
	char title[32]{};
	std::uint16_t count{0};
	std::uint16_t version{3U};
};

inline constexpr std::uint16_t maxTreePatchOperations = 16U;

struct TreeUpdateHeader {
	std::uint16_t count{0};
	std::uint16_t version{1U};
};

struct TreePatchHeader {
	std::uint16_t count{0};
	std::uint16_t version{1U};
};

struct TreePatchOperation {
	std::uint16_t kind{0}; // 1 insert, 2 remove subtree, 3 reparent
	std::uint16_t id{0};
	std::uint16_t parentId{0};
	std::uint16_t reserved{0};
	TreeNode node{};
};

struct UiCapabilities {
	std::uint16_t protocolVersion{3U};
	std::uint16_t maxNodes{0};
	std::uint16_t maxControls{0};
	std::uint16_t flags{1U}; // bit 0: counted v3 tree
};

struct TreeDocument {
	char title[32]{};
	std::uint16_t count{0};
	std::uint16_t version{2U};
	TreeNode nodes[maxTreeNodes]{};
};

// Private resolved state, never copied across an application boundary.
struct ResolvedDocument {
	char title[32]{};
	std::uint16_t count{0};
	std::uint16_t version{1U};
	Control controls[maxResolvedControls]{};
};

static_assert(sizeof(TreeNode) == 52);
static_assert(sizeof(TreeDocument) == 868);
static_assert(sizeof(TreeHeaderV3) == 36);
static_assert(sizeof(UiCapabilities) == 8);
static_assert(sizeof(TreePatchHeader) == 4);
static_assert(sizeof(TreeUpdateHeader) == 4);
static_assert(sizeof(TreePatchOperation) == 60);

} // namespace lexxos::ui
