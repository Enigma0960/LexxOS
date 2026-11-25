# LexxUI S5.10 — managed TreeWindowV3 mutations

`TreeWindowV3<Capacity>` now keeps its SDK registry aligned with atomic
v3 patches. The UI system remains the source of truth; no registry mutation
occurs if a structural patch is rejected.

- `window.add(widget, parent)` and `window.add(group, parent)` append before
  `show()` as before; after `show()` they send an insertion patch and attach
  the widget only after successful host confirmation.
- `window.remove(id)` removes a node and all descendants; their live widgets
  are detached after the system accepts the deletion.
- `window.reparent(id, parent)` moves an existing node to another layout
  group (`nullptr` places it at the root). Cycles are rejected locally;
  a rejected host patch does not change the local parent relation.

These are **individual atomic structural edits**, not a client-side
multi-operation transaction builder. The underlying `TreePatchBatch` and
`Transport::patch_tree_v3()` remain available for callers needing batching.
Do not mix raw patches with `TreeWindowV3`'s managed registry: a raw patch
bypasses object registration and callback lifetime handling.

### Ownership and failure semantics

Widgets and layout groups remain non-owning references. A removed widget
may be destroyed only after `remove()` returns success. A failed removal
leaves widgets and their event handlers attached. `add()` returns `false`
on a rejected host insertion, while `remove()` and `reparent()` return the
host status (`0` on success). A widget attach callback that fails **after**
a successful insertion closes the entire window to avoid an inconsistent
client registry.

The OS still performs a complete render after each accepted patch. On a
hardware rendering failure it may have touched a framebuffer, even though
the logical UI session and SDK registry remain unchanged.

### Example

```cpp
lexx::ui::TreeWindowV3<32> window{"Configuration", transport};
lexx::ui::Column root{100};
lexx::ui::Column advanced{101};
lexx::ui::Button apply{1, "Apply"};
window.add(root);
window.add(advanced, &root);
window.show();
window.add(apply, &advanced); // live atomic insert
window.reparent(apply.id(), &root);
window.remove(advanced.id());
```

Tests: `lexxos_ui_managed_tree_tests` covers live insertion, removal,
reparenting, rejected updates, event detachment and close/reopen behavior.
