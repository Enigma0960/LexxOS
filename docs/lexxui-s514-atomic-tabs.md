# LexxUI S5.14 — one-frame atomic tab switches

## Motivation

S5.13 Tabs removed the old page, inserted the new one, then sent a
separate state update for raised tab markers. All individual operations
were atomic but the sequence could display an intermediate empty page.
S5.14 stages all edits locally and sends one graphics operation 15.

## Wire ABI and compatibility

`TreePatchHeader` remains 4 bytes and `TreePatchOperation` remains 60
bytes. Its `kind` field now accepts 4 in addition to the existing
insert (1), remove subtree (2), and reparent (3) operations.

Kind 4 updates the **visual/disabled flag byte** on an existing node:

- `id` is the stable ID and `node.control.id` must equal it;
- `node.control.kind` must match the existing node;
- layout bits 1 and 2 (0x06) cannot change;
- allowed flags are 0x0F; the active-tab bit 0x08 is valid only for Button;
- all other node fields are ignored.

The system applies kind 4 to the same *private candidate* as the
structural changes, resolves geometry, renders once, then commits.
No new syscall or record layout is introduced. Previous applications
continue to use their existing operations unchanged.

Capability flag 0x04 (`capability_atomic_patch_flags`) advertises this
extension; the service reports flags 0x07. Existing applications can
still use the earlier, separate tree-update operation.

## SDK transaction

`TreeWindowV3` now provides:

```cpp
if (window.beginBatch() == 0) {
    // Existing add/remove/reparent calls stage local changes only.
    // Page and tab button descriptors can be updated here.
    int status = window.commitBatch();  // One tree patch, one render.
    if (status != 0) {
        // The local registry has been restored automatically.
    }
}
```

`rollbackBatch()` discards staged structural changes. No new widget
is activated or old widget deactivated until a successful commit.
Failed OS validation leaves the existing view, focus, and list stores
untouched. If a widget's attach handler fails *after* the OS commit,
the entire window is closed because automatic rollback is no longer
possible. This is an explicit limit of the SDK model.

The batch encoder orders operations as subtree removals, insertions,
reparents and visual flag updates. It rejects batches above 16 wire
operations before sending anything. A removed ID cannot be reinserted
in the same batch, as in S5.9. The current SDK does not support nested
batches or transactions that mix structural changes with arbitrary
changes to text/value.

`Tabs::select()` automatically starts a batch for an already-open
window, stages both page callbacks, updates each tab marker and commits
once. Before the initial `show()` it constructs the tree directly.

## Testing

- `lexxos_ui_options_tests`: one graphics/15 request for switch,
  no graphics/16 request, rollback on rejected patch.
- `lexxos_ui_tree_tests`: combined remove/insert/flag patch, invalid
  flags and immutability of input tree.
- `full-dev-sim`: open `ui-demo.lexx`, switch tabs with keyboard,
  confirm the old page never flashes between frames.
- On Lyra/PicoCalc measure actual prepared/presented frames and
  transferred SPI bytes while switching pages.

The first transition on a blank window still uses the normal v3
`show()` request. List widgets may call `set_items` in their attach
handlers **after** structural commit; these additional data requests
can cause later redraws, and the first committed frame can display
an empty list temporarily. A truly all-in-one page/data transaction
will need a combined item payload or a deferred-render attach API.

A driver-specific display error can partially change a framebuffer
even if the UI session transaction is rolled back; correcting this
requires a separate display commit contract.
