# LexxUI S5.13: Independent list item stores and raised selected tabs

## Item storage

GuestView now owns bounded item pages indexed by control ID, rather than
one global ListItems record. The v1/v2 wire structs remain unchanged.
Each list widget continues to use an eight-entry transport page, while
PagedSelection maintains its larger logical list on the application side.

The selected memory profile provides `ui.maxListStores`:

- Base/host and Lyra: 8 concurrently active list controls.
- Pico platform: 4 concurrently active list controls.

The count is validated at tree configuration, before a window becomes
active. Adding a ninth list to an eight-store profile is rejected before
committing any structural patch. The legacy v1/v2 one-list limit remains
unchanged. v3 supports multiple independent ScrollList, Dropdown and
ListView controls in a single window.

Stores are allocated in a fixed-size array as part of GuestView. Their
content follows surviving matching-ID/matching-kind controls during an
atomic tree patch, and is removed with deleted widgets. `setItems`
validates the target type and each string before updating its own slot.

The common demo's List tab now contains both ScrollList and Dropdown,
providing a runtime check that page changes do not overwrite a second list.

## Selected-tab presentation

The SDK's `Button::set_tab_active(bool)` marks tab selection independently
of keyboard focus. Since S5.14, `Tabs::select()` changes the page structure
and tab markers within **one** operation 15 transaction for live windows;
there is no separate `update(tabBar)` syscall after a page switch.
Before `show()` markers are included directly in the initial tree document.

The kernel renderer paints active tab buttons 3 pixels higher than ordinary
buttons with a distinct background. Focus still takes precedence for the
focus color. Clipping/dirty area calculations include the raised 3-pixel
edge in both old and new tab states.

## Verification

Run `lexxos_ui_tree_tests` and the existing `lexxos_ui_options_tests`;
then build `full-dev-sim` and open `ui-demo.lexx`. Check arrow navigation
of both controls on the first page and switch tabs while focus stays on a
different button. On Pico and Lyra measure retained GuestView memory use;
this change intentionally trades a bounded amount of host RAM for independent
lists. The list store limit is profile-controlled and does not change the
existing wire ABI.
