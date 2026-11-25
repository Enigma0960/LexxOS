# LexxUI S5.12 — selection controls and single gallery

## Compatibility and resource limits

The new leaf kinds are ScrollList (10), Dropdown (11) and RadioButton (12).
The v1 Document ABI and fixed v2 TreeDocument ABI are unchanged. New kinds
are intended for v3 apps using the common OS renderer.

The system continues to support one active ListItems table per window. Both
ScrollList and Dropdown consume it, so do not mount them together in the same
active page. The gallery switches them through atomic v3 tree operations.
The backend list page remains at most eight entries; the SDK shows pages
of four rows for ScrollList or two for Dropdown, respecting smaller
screens. Caller-provided strings remain owned by the application and must
outlive the control. The SDK caps logical selection lists at 256 entries
without dynamic allocation.

## API

- `ScrollList::set_items(ptrs, count)`: bounded virtual list with Up/Down
  selecting and shifting the visible page at either edge.
- `Dropdown::set_items(ptrs, count)`: press Enter to expand/collapse, use
  Up/Down to select; selection events retain the expanded-state bit internally.
- `CheckboxGroup<N>`: non-owning Column of independent Checkbox widgets;
  `selected_mask()` reports up to 32 selections.
- `RadioButton` and `RadioGroup<N>`: radio-marked controls; choosing one
  deselects the others.
- `Tabs<WindowCapacity, MaxTabs>`: non-owning tab bar and mount/unmount
  callbacks, allowing inactive pages to use no active tree slots.

The `TreeWindowV3` atomic insert/remove operations back mounted pages.
Page callbacks should return false if they cannot mount the requested
tree. Callbacks and widget lifetimes are application-owned.

The old `ui-tree-demo.lexx` target is no longer built by default. The
unified `ui-demo.lexx` and `ui-native-demo` present the same tabbed
gallery. The old native tree command remains a compatibility alias.

## Testing

Build and run `lexxos_ui_options_tests` for page updates, selection and
radio/checkbox group behavior. On host simulator test keyboard focus,
list boundary transitions, changing tabs, switching radios, and the
dropdown on the same active page (but not concurrently with ScrollList).
Test on a 320x320 and 128x160 display, including scroll/focus movement.

The radio mark is presently rendered using RGB565 fills; this is a basic
embedded-style control, not a vector antialiased circle. Multi-selection
checkboxes are independent rather than a dedicated multiselect wire type.

Memory and rendering should be benchmarked on board. `ListItems` still
transmits a full 260-byte record for each page shift. A future version
can add per-control item stores and virtualization inside the kernel.
