# LexxUI S5.11 — batched updates and pixel clipping

Graphics operation 16 accepts a counted update packet: `count:uint16`,
`version:uint16=1`, followed by `Control[count]`. The exact byte count is
`4 + count * 48`. Legacy graphics operations remain unchanged.

`TreeWindowV3::update()`, `update(id)` and `update(group)` collect widget
descriptors from the selected subtree and submit them through one transport
operation. The host updates a private copy of the view; invalid or duplicate
IDs abort the entire batch without modifying the active view. The update
covers values, labels and enabled states; it does not change widget geometry.

The renderer computes dirty rectangles by comparing the old and new resolved
controls, including focus changes and list contents. Up to eight regions are
coalesced. `ClippedDisplay` intercepts fill and RGB565 pixel/alpha blits and
clips writes to the current dirty region. All regions share a single physical
beginFrame/endFrame pair. A full redraw is used for viewport or scroll extent
changes, title or modal dialog changes, or region overflow.

A display backend must preserve pixels outside updated rectangles between
frames. When using non-retained hardware framebuffers, do not enable the
partial rendering path until the backend can guarantee this property.

Run `lexxos_ui_clipping_tests` and `lexxos_ui_tree_tests` and test the guest
and native demos on the simulator. Measure actual SPI transfer bytes and
compare before/after, not merely reported FPS.
