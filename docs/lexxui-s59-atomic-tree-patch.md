# LexxUI S5.9 — atomic v3 tree patches

The counted v3 document remains supported with graphics operation 14.
Operation **15** accepts a bounded batch of 1..16 structural operations.
Old v1/v2 records and operation numbers are unchanged.

## Wire contract

`TreePatchHeader` is 4 bytes: `count:uint16`, `version:uint16=1`.
Each operation is 60 bytes: `kind:uint16`, `id:uint16`, `parentId:uint16`,
`reserved:uint16=0` and a 52-byte `TreeNode`. The third graphics syscall
argument is the exact payload size `4 + count * 60`.

- Kind 1: insert a new node; `node.control.id == id`, `node.parentId == parentId`.
- Kind 2: remove an existing node and all its descendants.
- Kind 3: move an existing node under `parentId` (zero denotes root).
- Kind 4 (S5.14): atomically update the visual/disabled flags of an existing
  node without changing kind or geometry. See
  [S5.14 atomic tabs](lexxui-s514-atomic-tabs.md).

Operations are evaluated **in order** against a private candidate. The
candidate is reordered parent-before-child, then passed through the same
v3 geometry and structural validator. Invalid IDs, cycles, missing parents,
exceeded session limits and invalid geometry reject the **whole** batch.
A removed ID cannot be reused by an insertion in the same batch; this
prevents a newly constructed widget from inheriting stale state.

The system retains one source-tree snapshot per active v3 UI session.
It commits that snapshot only if the candidate has been accepted and drawn.
The current focus is restored by ID when the corresponding control survives;
other unchanged controls retain their current text, value and disabled flag.
The current list entries are also retained if the same ListView survives.
Dialogs block structural updates. A full new `show()` replaces the snapshot;
`close()` frees it. UI session state is transactional, although a hardware
render failure can leave a transient partially drawn framebuffer.

## Guest example

```cpp
lexx::ui::TreePatchBatch<2> batch{};
batch.header.count = 2;
batch.operations[0].kind = 1;
batch.operations[0].id = 40;
batch.operations[0].parentId = 100;
batch.operations[0].node.control.id = 40;
batch.operations[0].node.control.kind = 2;
batch.operations[0].node.control.flags = lexx::ui::flag_auto_layout;
batch.operations[0].node.parentId = 100;
batch.operations[1].kind = 3;
batch.operations[1].id = 3;
batch.operations[1].parentId = 100;
int status = lexx::ui::patch_tree_v3(batch);
```

The native transport also exposes `Transport::patch_tree_v3(batch)`.
`UiCapabilities.flags` bit 1 advertises atomic patches; check this bit
before sending operation 15. The existing `TreeWindowV3` object registry
is **not** automatically changed by raw patches; users who mutate a window
through the low-level patch API must keep its own widget registry consistent.
A higher-level managed tree builder is a separate SDK step.

## Scope and verification

The patch now supports visual tab flags (S5.14) as well as structural
mutation. Dirty-region rendering may be used when supported by the
display. It is not a complete retained scene-graph renderer.
Run `lexxos_ui_tree_tests` to check insertion, subtree deletion, reparent,
cycles, unknown IDs, and failed-batch rollback, then test native and guest
rendering in the simulator. On resource-constrained hardware measure heap
headroom: a patch temporarily requires a candidate node array, one sorting
array and a prospective `GuestView`.
