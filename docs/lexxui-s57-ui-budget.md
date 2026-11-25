# LexxUI S5.7 — UI resource budgets (first increment)

## Compatibility

This increment preserves v1 `Document` (420 bytes), v2 `TreeDocument`
(868 bytes), widget types, syscall numbering, and the fixed maximum of
16 **wire nodes** (including containers). Existing `.lexx` applications do
not need recompilation.

The first resource optimization concerns the **private resolved-control
array**, which was always sized for 16 leaf controls even on small boards.

## Build-time SRAM limit

The resolved control capacity is selected by
`runtime::standardRuntimeProfile.ui.maxResolvedControls` using the existing
`config/memory/` profile layering (chip, platform, board). The global CMake
cache variable `LEXXOS_UI_RESOLVED_CONTROL_CAPACITY` has been removed.

The base profile now provides 32 resolved controls and 32 tree nodes;
the Pico platform uses 12 controls / 16 nodes, PicoCalc Pico 2 W uses
16 / 16, and PicoCalc Lyra uses 64 / 64. Both values are compile-time
capacities, while each UI session may enforce stricter limits.
A board can override its platform value, for example:

```cpp
constexpr void applyMyBoard(RuntimeProfile& profile) {
    profile.ui.maxResolvedControls = 10;
}
```

The compiled value sizes `ResolvedDocument::controls[]` embedded in each
`GuestView`/`UiSession`. Existing v1 windows need space for eight controls,
so profile validation rejects a limit below eight.
The v2 wire protocol still accepts at most 16 nodes. The separate v3
counted-tree operation supports larger documents up to the selected board
profile's maximum, with a maximum compile-time safety ceiling of 256.

Target-specific profile compile definitions must be consistent across all
translation units using `GuestView`, as with other compile-time array sizes.

A v2 document still carries up to 16 nodes on the wire. The tree resolver
rejects a document that would overflow the compiled private control pool,
while allowing additional non-rendering Column/Row container nodes.

This saves `48 * (16 - capacity)` bytes per resolved control array, excluding
alignment effects and any transient copies. For capacity 10 the direct
array saving is 288 bytes per `GuestView`. It does not reduce the on-wire
tree buffer or introduce heap allocations.

## Per-process UI admission

The host-side `UiSession` owns an independent `UiBudget` containing
`maxNodes` (1..16) and `maxControls` (1..compiled capacity).

The host may call `session.configureBudget(requested)` before showing a
window. Invalid policies and changes while a view is active are rejected.
`UiService::showTree` checks the node limit and leaf-control limit before
resolving the document. Existing tree validation still rejects unknown
types, illegal parents and other malformed inputs. `UiService::show`
enforces the control limit for legacy windows.

This is **admission control**, not runtime memory accounting or an actual
variable-sized memory allocator. Session policies are configured by trusted
host code, not by guest applications.

## Regression checks

The existing S5.6 13-node test tree contains ten leaf controls. Tests
now verify that a build configured with a pool below ten rejects it
without an out-of-bounds write, and that the default pool still accepts it.
They also check invalid budget values.

```sh
cmake --build build/host-full --target lexxos_ui_tree_tests
ctest --test-dir build/host-full -R '^lexxos_ui_tree_tests$' --output-on-failure
```

## Remaining roadmap

1. Queryable UI capabilities and resource limits for native/RV32I clients.
2. Stable-ID structural change batches with atomic validation.
3. Clipping/scissor and dirty-rectangle rendering.
4. Multiple lists with virtualization/pagination.

Increasing wire capacity above 16 is a **separate protocol revision**:
it must not be done by changing the v2 on-wire `TreeDocument` layout.
