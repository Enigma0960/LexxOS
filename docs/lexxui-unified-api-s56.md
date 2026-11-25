# LexxUI S5.6 — UI protocol v2: nested layouts (first increment)

## Base and compatibility

This patch builds on S5.5 (`UiService`, `UiSession`, `NativeTransport` and the shared
object SDK). The v1 `Document`, `Control`, `Event`, `ListItems` and operations
6–11 are unchanged. New application code must explicitly opt into protocol v2.
Old `.lexx` images do not require repacking.

## What is implemented

- New operation `graphics/12`: copy and validate one `TreeDocument` (868 bytes)
  from guest memory, then submit it through the same `UiService` used by native
  clients. No guest pointers or callbacks are stored by the kernel.
- New `Transport::show_tree` operation. Native and guest transports use the same
  object descriptions and validation logic.
- `TreeWindow`, `Column`, `RowContainer` in `apps/sdk/lexx/ui/tree_window.h`.
  Existing flat `Window` and legacy `Row` continue to work.
- System-owned nested layout (columns and rows, up to four group levels),
  keyboard focus, scrolling, and existing modal `MessageBox`. Controls continue
  using their own existing handlers; containers are geometry only and do not
  consume focus.
- An independent new `ui-tree-demo.lexx` and `ui-native-tree-demo` driven by
  `shared_tree_demo.h`. The native command is gated by the existing
  `LEXXOS_FEATURE_UI_NATIVE_DEMO` feature, default OFF.

### Example (shared source for native and guest)

```cpp
lexx::ui::Column root{100};
lexx::ui::Column settings{101};
lexx::ui::RowContainer actions{102};
lexx::ui::TextField name{1, "PicoCalc"};
lexx::ui::Button save{2, "Save"};
lexx::ui::Button close{3, "Exit"};
lexx::ui::TreeWindow window{"Settings", transport};

window.add(root);
window.add(settings, &root);
window.add(name, &settings);
window.add(actions, &root);
window.add(save, &actions);
window.add(close, &actions);
return window.exec();
```

The `TreeWindow` is **non-owning**: all layout containers and widgets must
remain alive until the window is closed. Parents must have been added before
children. IDs are unique across containers and widgets. Native transport does
not bypass system validation or graphics ownership checks.

## On-wire contract v2

`TreeDocument` contains a 32-byte title, `count`, `version=2`, and 16 fixed
`TreeNode` records. Each node contains the existing 48-byte `Control`, an
unsigned 16-bit `parentId` (0 means the implicit root column), and a 16-bit
reserved field that must be zero. Wire sizes: `TreeNode=52`,
`TreeDocument=868` bytes. All strings are bounded UTF-8, null-terminated.

Node types 1–7 retain the v1 meaning. Types 8 (`Column`) and 9 (`Row`) are
containers, not independently rendered or focusable controls. Geometry is
computed by the OS; explicit pixel coordinates and legacy `sameRow` flags
are rejected by the v2 validator. Layout dimensions use screen width and
system margin/gap constants; no allocation occurs during resolution.

The OS rejects: duplicate or zero IDs, forward/non-container references,
unknown kinds, oversized documents, nonzero reserved fields, invalid string
termination, too-deep nesting and out-of-range geometry. The prospective
view is validated and drawn before replacing the active UI session.

## Resource constraints and deliberate limitations

- Total **16 nodes** including containers, depth at most **four** nested
  groups. This doubles the available node count relative to v1, but is not a
  general unbounded scene graph.
- The system still has **one ListView item table of at most eight entries**
  per window, the v1 character buffer per control (31 bytes + NUL), and
  keyboard-driven selection.
- V2 supports tree construction on `show` and updating existing leaf controls
  with the existing operation 9. Structural mutation (insert/remove/reparent)
  after show is deferred to a later v2 increment.
- When a widget is only partially visible, it is intentionally not rendered.
  Universal clipping/scissor support is a subsequent stage.
- All views are fullscreen, single-session foreground UI. Multiple overlapping
  top-level windows, mouse/touch hit testing, font manager and long UTF-8 text
  input are future work.

### Important caveat

The `UiSession` stores a larger private resolved document (up to 16 controls),
so its memory footprint increases even for v1 clients. Confirm the effects on
RP2040 and other SRAM-constrained targets before enabling additional sessions.
No new dynamic allocations were introduced.

## Build and functional tests

```sh
cmake --preset full-dev-sim -DLEXXOS_FEATURE_UI_NATIVE_DEMO=ON
cmake --build --preset full-dev-sim --parallel 2

# When using a host-test preset containing tests/cpp:
cmake --build build/host-full --target lexxos_ui_tree_tests -j 12
ctest --test-dir build/host-full -R '^lexxos_ui_tree_tests$' --output-on-failure
```

Build standalone guest applications with `cmake --build build/host-full
--target app-ui-tree-demo`, or the corresponding `apps` preset. Copy
`build/apps/ui-tree-demo.lexx` to the emulator's SD directory. Restart the
simulator after recompiling the host library.

Run both variants in the shell and compare focus and rendering:

```text
lexx:/> ui-native-tree-demo
lexx:/> /mnt/sd/lexx/ui-tree-demo.lexx
```

Test `Tab` navigation through more than eight controls, `Slider` updates to
`ProgressBar`, scrolling, row alignment, and the `Confirm` question's OK/Cancel
paths. Verify `Esc` closes the window and returns graphics ownership to the
shell. Verify existing `ui-native-demo` and `ui-demo.lexx` still operate.

## Next S5.6 increments

1. Node-pool budgeting per board and per-process UI session, not a global
   fixed count; explicit feature capability/version queries.
2. Incremental subtree patch/remove operations with stable IDs and atomic
   batches; avoid full re-show when a layout changes.
3. Clipping/scissor and dirty-rectangle rendering.
4. Generalized list models with pagination/virtualization and multiple lists.
5. Text metrics and Unicode editing, pointer navigation and font manager.
