#pragma once

#include <lexx/ui/tree_window.h>

namespace lexx::ui {

// Non-owning v3 object API. Capacity is a caller-chosen storage budget,
// not a fixed protocol limit; compile only as many nodes as the app needs.
template <uint16_t Capacity>
class TreeWindowV3 final {
public:
	TreeWindowV3(char const* title, Transport const& transport)
	    : _transport(transport) {
		for (uint16_t i = 0; title != nullptr && title[i] != '\0' && i < 31U; ++i) {
			_document.header.title[i] = title[i];
		}
	}

#if !defined(LEXXOS_UI_NATIVE)
	explicit TreeWindowV3(char const* title)
	    : TreeWindowV3(title, guestTransport) {}
#endif

	TreeWindowV3(TreeWindowV3 const&) = delete;
	TreeWindowV3& operator=(TreeWindowV3 const&) = delete;

	~TreeWindowV3() {
		if (_open) {
			(void) close();
		}
	}

	[[nodiscard]] bool opened() const { return _open; }

	// Stage structural edits locally. commitBatch() sends one atomic v3 patch;
	// rollbackBatch() restores the original registry without host calls.
	int beginBatch() {
		if (!_open || _staging) {
			return -1;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			_originalEntries[i] = _entries[i];
			if (_entries[i].widget != nullptr) {
				_originalControls[i] = _entries[i].widget->descriptor();
			}
		}

		_originalCount = _count;
		_originalLeafCount = _leafCount;
		_staging = true;
		return 0;
	}

	void rollbackBatch() {
		if (!_staging) {
			return;
		}

		for (uint16_t i = 0; i < _originalCount; ++i) {
			_entries[i] = _originalEntries[i];
		}

		_count = _originalCount;
		_leafCount = _originalLeafCount;
		_staging = false;
	}

	[[nodiscard]] int commitBatch() {
		if (!_staging) {
			return -1;
		}

		TreePatchBatch<max_tree_patch_operations> batch{};
		auto append = [&](uint16_t kind, uint16_t id, uint16_t parentId,
		                  Control const* descriptor) -> bool {
			if (batch.header.count >= max_tree_patch_operations) {
				return false;
			}

			auto& op = batch.operations[batch.header.count++];
			op.kind = kind;
			op.id = id;
			op.parentId = parentId;
			if (descriptor != nullptr) {
				op.node.control = *descriptor;
				op.node.parentId = parentId;
			}

			return true;
		};

		auto originalIndex = [&](uint16_t id) -> int {
			for (uint16_t i = 0; i < _originalCount; ++i) {
				if (entryId(_originalEntries[i]) == id) {
					return i;
				}
			}

			return -1;
		};
		auto currentIndex = [&](uint16_t id) -> int {
			for (uint16_t i = 0; i < _count; ++i) {
				if (entryId(_entries[i]) == id) {
					return i;
				}
			}

			return -1;
		};

		// Remove only subtree roots; descendants are removed by the kernel.
		for (uint16_t i = 0; i < _originalCount; ++i) {
			auto const& old = _originalEntries[i];
			if (currentIndex(entryId(old)) >= 0) {
				continue;
			}

			if (old.parentId != 0U && currentIndex(old.parentId) < 0) {
				continue;
			}

			if (!append(2U, entryId(old), 0U, nullptr)) {
				rollbackBatch();
				return -1;
			}
		}

		// New entries are recorded parent-before-child in the local registry.
		for (uint16_t i = 0; i < _count; ++i) {
			auto const& entry = _entries[i];
			if (originalIndex(entryId(entry)) >= 0) {
				continue;
			}

			Control descriptor{};
			if (entry.widget != nullptr) {
				descriptor = entry.widget->descriptor();
			} else {
				descriptor.id = entryId(entry);
				descriptor.kind = entry.group->kind();
				descriptor.flags = flag_auto_layout;
			}

			if (!append(1U, entryId(entry), entry.parentId, &descriptor)) {
				rollbackBatch();
				return -1;
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			auto const& entry = _entries[i];
			int const previous = originalIndex(entryId(entry));
			if (previous < 0) {
				continue;
			}

			if (entry.parentId != _originalEntries[previous].parentId && !append(3U, entryId(entry), entry.parentId, nullptr)) {
				rollbackBatch();
				return -1;
			}

			if (entry.widget != nullptr && entry.widget->descriptor().flags != _originalControls[previous].flags && !append(4U, entryId(entry), entry.parentId, &entry.widget->descriptor())) {
				rollbackBatch();
				return -1;
			}
		}

		int const status = batch.header.count == 0U
		    ? 0
		    : _transport.patch_tree_v3(batch);
		if (status != 0) {
			rollbackBatch();
			return status;
		}

		_staging = false;
		// Deactivation and attachment happen only after the OS commit.
		for (uint16_t i = 0; i < _originalCount; ++i) {
			auto const& previous = _originalEntries[i];
			if (previous.widget != nullptr && currentIndex(entryId(previous)) < 0) {
				previous.widget->deactivate();
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			auto& entry = _entries[i];
			if (entry.widget != nullptr && originalIndex(entryId(entry)) < 0) {
				int const activated = entry.widget->activate(_transport);
				if (activated != 0) {
					(void) close(); // Kernel committed, so rollback is no longer possible.
					return activated;
				}
			}
		}

		return 0;
	}

	bool add(LayoutGroup& group, LayoutGroup const* parent = nullptr) {
		return _open ? insertLive(nullptr, &group, parent) == 0 : insert(nullptr, &group, parent);
	}

	bool add(Widget& widget, LayoutGroup const* parent = nullptr) {
		return _open ? insertLive(&widget, nullptr, parent) == 0 : insert(&widget, nullptr, parent);
	}

	// Structural edits are committed to the local registry only after the OS
	// confirms the atomic patch. The caller retains ownership of all objects.
	int remove(uint16_t id) {
		if (!_open || id == 0U) {
			return -1;
		}

		bool removed[Capacity]{};
		bool found = false;
		for (uint16_t i = 0; i < _count; ++i) {
			removed[i] = entryId(_entries[i]) == id;
			found |= removed[i];
		}

		if (!found) {
			return -1;
		}

		// Determine the entire subtree before touching either the OS or registry.
		for (uint16_t pass = 0; pass < _count; ++pass) {
			bool changed = false;
			for (uint16_t i = 0; i < _count; ++i) {
				if (removed[i]) {
					continue;
				}

				for (uint16_t j = 0; j < _count; ++j) {
					if (removed[j] && _entries[i].parentId == entryId(_entries[j])) {
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

		if (_staging) {
			uint16_t remaining = 0;
			for (uint16_t i = 0; i < _count; ++i) {
				if (removed[i]) {
					_leafCount -= _entries[i].widget != nullptr ? 1U : 0U;
				} else {
					_entries[remaining++] = _entries[i];
				}
			}

			_count = remaining;
			return 0;
		}

		TreePatchBatch<1> patch{};
		patch.header.count = 1U;
		patch.operations[0].kind = 2U;
		patch.operations[0].id = id;
		int const status = _transport.patch_tree_v3(patch);
		if (status != 0) {
			return status;
		}

		uint16_t remaining = 0;
		for (uint16_t i = 0; i < _count; ++i) {
			if (removed[i]) {
				if (_entries[i].widget != nullptr) {
					_entries[i].widget->deactivate();
					--_leafCount;
				}
			} else {
				_entries[remaining++] = _entries[i];
			}
		}

		_count = remaining;
		return 0;
	}

	// Resynchronize a node and all descendant widgets in one batch.
	// The OS computes and clips the resulting dirty regions.
	int update(uint16_t id = 0U) {
		if (!_open) {
			return -1;
		}

		bool rootFound = id == 0U;
		for (uint16_t i = 0; i < _count; ++i) {
			rootFound |= entryId(_entries[i]) == id;
		}

		if (!rootFound) {
			return -1;
		}

		TreeUpdateBatch<Capacity> batch{};
		for (uint16_t i = 0; i < _count; ++i) {
			bool inside = id == 0U || entryId(_entries[i]) == id;
			uint16_t parentId = _entries[i].parentId;
			for (uint16_t depth = 0; !inside && parentId != 0U && depth < _count; ++depth) {
				inside = parentId == id;
				for (uint16_t j = 0; j < _count; ++j) {
					if (entryId(_entries[j]) == parentId) {
						parentId = _entries[j].parentId;
						break;
					}
				}
			}

			if (inside && _entries[i].widget != nullptr) {
				batch.controls[batch.header.count++] = _entries[i].widget->descriptor();
			}
		}

		return batch.header.count != 0U ? _transport.update_tree_batch(batch) : 0;
	}

	int update(LayoutGroup const& group) { return update(group.id()); }

	int update(Widget const& widget) { return update(widget.id()); }

	int reparent(uint16_t id, LayoutGroup const* parent = nullptr) {
		if (!_open || id == 0U) {
			return -1;
		}

		int index = -1;
		for (uint16_t i = 0; i < _count; ++i) {
			if (entryId(_entries[i]) == id) {
				index = i;
				break;
			}
		}

		if (index < 0) {
			return -1;
		}

		uint16_t const parentId = parent == nullptr ? 0U : parent->id();
		if (parent != nullptr) {
			bool exists = false;
			for (uint16_t i = 0; i < _count; ++i) {
				exists |= _entries[i].group == parent;
			}

			if (!exists) {
				return -1;
			}
		}

		// Reject cycles locally, even when the remote implementation is mocked.
		uint16_t ancestor = parentId;
		for (uint16_t depth = 0; ancestor != 0U && depth < _count; ++depth) {
			if (ancestor == id) {
				return -1;
			}

			bool exists = false;
			for (uint16_t i = 0; i < _count; ++i) {
				if (entryId(_entries[i]) == ancestor) {
					ancestor = _entries[i].parentId;
					exists = true;
					break;
				}
			}

			if (!exists) {
				return -1;
			}
		}

		if (_entries[index].parentId == parentId) {
			return 0;
		}

		if (_staging) {
			_entries[index].parentId = parentId;
			normalizeEntries();
			return 0;
		}

		TreePatchBatch<1> patch{};
		patch.header.count = 1U;
		patch.operations[0].kind = 3U;
		patch.operations[0].id = id;
		patch.operations[0].parentId = parentId;
		int const status = _transport.patch_tree_v3(patch);
		if (status == 0) {
			_entries[index].parentId = parentId;
			normalizeEntries();
		}

		return status;
	}

private:
	// Keep parent-before-child order for close()/show() on a moved tree.
	void normalizeEntries() {
		Entry ordered[Capacity]{};
		bool emitted[Capacity]{};
		for (uint16_t position = 0; position < _count; ++position) {
			for (uint16_t i = 0; i < _count; ++i) {
				if (emitted[i]) {
					continue;
				}

				bool ready = _entries[i].parentId == 0U;
				for (uint16_t j = 0; j < position && !ready; ++j) {
					ready = ordered[j].group != nullptr && entryId(ordered[j]) == _entries[i].parentId;
				}

				if (ready) {
					emitted[i] = true;
					ordered[position] = _entries[i];
					break;
				}
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			_entries[i] = ordered[i];
		}
	}

	template <typename T>
	static uint16_t entryId(T const& entry) {
		return entry.widget != nullptr ? entry.widget->id() : entry.group->id();
	}

	int insertLive(Widget* widget, LayoutGroup* group, LayoutGroup const* parent) {
		uint16_t const id = widget != nullptr ? widget->id() : group->id();
		if (!_open || id == 0U || _count >= Capacity || (widget != nullptr && widget->attached())) {
			return -1;
		}

		uint16_t const parentId = parent == nullptr ? 0U : parent->id();
		if (parent != nullptr) {
			bool found = false;
			for (uint16_t i = 0; i < _count; ++i) {
				found |= _entries[i].group == parent;
			}

			if (!found) {
				return -1;
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (entryId(_entries[i]) == id || (widget != nullptr && _entries[i].widget == widget) || (group != nullptr && _entries[i].group == group)) {
				return -1;
			}
		}

		TreePatchBatch<1> patch{};

		if (_staging) {
			_entries[_count++] = {widget, group, parentId};
			_leafCount += widget != nullptr ? 1U : 0U;
			return 0;
		}

		patch.header.count = 1U;
		auto& operation = patch.operations[0];
		operation.kind = 1U;
		operation.id = id;
		operation.parentId = parentId;
		operation.node.parentId = parentId;
		if (group != nullptr) {
			operation.node.control.id = id;
			operation.node.control.kind = group->kind();
			operation.node.control.flags = flag_auto_layout;
		} else {
			operation.node.control = widget->descriptor();
			if ((operation.node.control.flags & flag_auto_layout) == 0U || (operation.node.control.flags & flag_same_row) != 0U) {
				return -1;
			}
		}

		int const status = _transport.patch_tree_v3(patch);
		if (status != 0) {
			return status;
		}

		_entries[_count++] = {widget, group, parentId};
		if (widget != nullptr) {
			++_leafCount;
			int const attached = widget->activate(_transport);
			if (attached != 0) {
				// A failed attach callback occurs after the OS commit. Closing
				// the window prevents a partially registered live document.
				(void) close();
				--_count;
				--_leafCount;
				return attached;
			}
		}

		return 0;
	}

public:
	int show() {
		if (_open) {
			return -1;
		}

		UiCapabilities capabilities{};
		if (_transport.query_capabilities(capabilities) != 0 || capabilities.protocolVersion < tree_protocol_v3 || capabilities.maxNodes < _count || capabilities.maxControls < _leafCount || (capabilities.flags & 1U) == 0U) {
			return -1;
		}

		_document.header.count = _count;
		for (uint16_t i = 0; i < _count; ++i) {
			auto& node = _document.nodes[i];
			auto const& entry = _entries[i];
			node.parentId = entry.parentId;
			if (entry.group != nullptr) {
				node.control.id = entry.group->id();
				node.control.kind = entry.group->kind();
				node.control.flags = flag_auto_layout;
			} else {
				node.control = entry.widget->descriptor();
				if ((node.control.flags & flag_auto_layout) == 0U || (node.control.flags & flag_same_row) != 0U) {
					return -1;
				}
			}
		}

		int const result = _transport.show_tree_v3(_document);
		if (result != 0) {
			return result;
		}

		_open = true;
		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr && _entries[i].widget->activate(_transport) != 0) {
				(void) close();
				return -1;
			}
		}

		return 0;
	}

	int poll() {
		if (!_open) {
			return -1;
		}

		Event event{};
		int const result = _transport.poll(event);
		if (result <= 0) {
			return result;
		}

		if (event.type == static_cast<uint16_t>(EventType::Close)) {
			(void) close();
			return 1;
		}

		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr && _entries[i].widget->id() == event.id) {
				_entries[i].widget->dispatch(event);
				break;
			}
		}

		return result;
	}

	int exec(uint32_t intervalMs = 20U) {
		if (!_open && show() != 0) {
			return -1;
		}

		while (_open) {
			if (poll() < 0) {
				(void) close();
				return -1;
			}

			if (_open) {
				_transport.sleep_ms(intervalMs);
			}
		}

		return 0;
	}

	int close() {
		if (_staging) {
			rollbackBatch();
		}

		if (!_open) {
			return 0;
		}

		_open = false;
		for (uint16_t i = 0; i < _count; ++i) {
			if (_entries[i].widget != nullptr) {
				_entries[i].widget->deactivate();
			}
		}

		return _transport.close();
	}

private:
	struct Entry {
		Widget* widget{nullptr};
		LayoutGroup* group{nullptr};
		uint16_t parentId{0};
	};

	bool insert(Widget* widget, LayoutGroup* group, LayoutGroup const* parent) {
		uint16_t const id = widget != nullptr ? widget->id() : group->id();
		if (_open || id == 0U || _count >= Capacity) {
			return false;
		}

		if (parent != nullptr) {
			bool found = false;
			for (uint16_t i = 0; i < _count; ++i) {
				found |= _entries[i].group == parent;
			}

			if (!found) {
				return false;
			}
		}

		for (uint16_t i = 0; i < _count; ++i) {
			uint16_t const other = _entries[i].widget != nullptr
			    ? _entries[i].widget->id()
			    : _entries[i].group->id();
			if (other == id) {
				return false;
			}
		}

		_entries[_count++] = {widget, group,
		    static_cast<uint16_t>(parent == nullptr ? 0U : parent->id())};
		_leafCount += widget != nullptr ? 1U : 0U;
		return true;
	}

	Transport const& _transport;
	TreeDocumentV3<Capacity> _document{};
	Entry _entries[Capacity]{};
	Entry _originalEntries[Capacity]{};
	Control _originalControls[Capacity]{};
	uint16_t _originalCount{0};
	uint16_t _originalLeafCount{0};
	bool _staging{false};
	uint16_t _count{0};
	uint16_t _leafCount{0};
	bool _open{false};
};

} // namespace lexx::ui
