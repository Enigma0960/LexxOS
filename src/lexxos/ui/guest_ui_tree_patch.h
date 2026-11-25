#pragma once

#include <cstdint>
#include <cstring>

#include <lexxos/ui/guest_ui_types.h>

namespace lexxos::ui::tree {

// All edits take place in caller-owned scratch space; no active tree is changed.
inline bool applyPatch(TreeHeaderV3 const& source, TreeNode const* sourceNodes,
    TreePatchHeader const& patch, TreePatchOperation const* operations,
    TreeNode* output, TreeNode* sorted, std::uint16_t capacity, std::uint16_t& count) {
	if (source.version != 3U || source.count > capacity || capacity > 256U || patch.version != 1U || patch.count == 0U || patch.count > maxTreePatchOperations || sourceNodes == nullptr || operations == nullptr || output == nullptr || sorted == nullptr) {
		return false;
	}

	count = source.count;
	std::memcpy(output, sourceNodes, count * sizeof(TreeNode));
	for (std::uint16_t operationIndex = 0; operationIndex < patch.count; ++operationIndex) {
		auto const& operation = operations[operationIndex];
		if (operation.reserved != 0U || operation.id == 0U) {
			return false;
		}

		int index = -1;
		for (std::uint16_t i = 0; i < count; ++i) {
			if (output[i].control.id == operation.id) {
				index = i;
				break;
			}
		}

		if (operation.kind == 1U) { // Insert node.
			// Reusing an ID removed earlier in the same batch would transfer
			// stale control state to a different widget. Reject that ambiguity.
			for (std::uint16_t original = 0; original < source.count; ++original) {
				if (sourceNodes[original].control.id == operation.id) {
					return false;
				}
			}

			if (index >= 0 || count >= capacity || operation.node.control.id != operation.id || operation.node.parentId != operation.parentId) {
				return false;
			}

			output[count++] = operation.node;
		} else if (operation.kind == 2U) { // Remove entire subtree.
			if (index < 0) {
				return false;
			}

			bool removed[256]{};
			removed[index] = true;
			for (std::uint16_t pass = 0; pass < count; ++pass) {
				bool changed = false;
				for (std::uint16_t i = 0; i < count; ++i) {
					if (removed[i]) {
						continue;
					}

					for (std::uint16_t parent = 0; parent < count; ++parent) {
						if (removed[parent] && output[i].parentId == output[parent].control.id) {
							removed[i] = true;
							changed = true;
							break;
						}
					}
				}

				if (!changed) {
					break;
				}
			}

			std::uint16_t remaining = 0;
			for (std::uint16_t i = 0; i < count; ++i) {
				if (!removed[i]) {
					output[remaining++] = output[i];
				}
			}

			count = remaining;
		} else if (operation.kind == 3U) { // Move to another parent.
			if (index < 0 || operation.parentId == operation.id) {
				return false;
			}

			output[index].parentId = operation.parentId;

		} else if (operation.kind == 4U) { // Update visual flags atomically with structure.
			if (index < 0 || operation.node.control.id != operation.id || operation.node.control.kind != output[index].control.kind || (operation.node.control.flags & ~0x0FU) != 0U || (operation.node.control.flags & 0x06U) != (output[index].control.flags & 0x06U) || ((operation.node.control.flags & 0x08U) != 0U && output[index].control.kind != 2U)) {
				return false;
			}

			output[index].control.flags = operation.node.control.flags;
		} else {
			return false;
		}
	}

	// Existing v3 validation requires parent-before-child order.
	bool emitted[256]{};
	for (std::uint16_t position = 0; position < count; ++position) {
		int candidate = -1;
		for (std::uint16_t i = 0; i < count; ++i) {
			if (emitted[i]) {
				continue;
			}

			if (output[i].parentId == 0U) {
				candidate = i;
				break;
			}

			for (std::uint16_t j = 0; j < position; ++j) {
				auto const& parent = sorted[j].control;
				if (parent.id == output[i].parentId && (parent.kind == 8U || parent.kind == 9U)) {
					candidate = i;
					break;
				}
			}

			if (candidate >= 0) {
				break;
			}
		}

		if (candidate < 0) {
			return false;
		}

		emitted[candidate] = true;
		sorted[position] = output[candidate];
	}

	std::memcpy(output, sorted, count * sizeof(TreeNode));
	return true;
}

} // namespace lexxos::ui::tree
