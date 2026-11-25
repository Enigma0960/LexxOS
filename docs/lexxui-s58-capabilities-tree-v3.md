# LexxUI S5.8 — capabilities and counted tree protocol v3

## Compatibility

Protocol v1 (`Document`, graphics operations 6–11) and v2
(`TreeDocument`, operation 12, 868-byte record) remain unchanged.
New graphics operation 13 reads an 8-byte `UiCapabilities` structure:
`protocolVersion`, `maxNodes`, `maxControls`, `flags` (all little-endian
`uint16_t`). Flag bit 0 advertises counted v3 tree support. The call
requires the graphics capability but does not claim the display.
An application's effective limits come from its `UiSession` budget;
the caller cannot raise them.

Operation 14 accepts a counted v3 tree. The third graphics syscall argument
is the **exact byte count**: `36 + nodeCount * 52`. The first 36 bytes
contain title[32], count (`uint16_t`) and version=3 (`uint16_t`).
Each subsequent 52-byte node has the same layout as v2, with `Control`
and `parentId`/`reserved`. The system first verifies the header bounds,
count, exact size and guest range, then copies the node sequence into
host-owned memory before invoking the shared tree validator.

Validation checks unique IDs, existing container parents, depth at most
four, valid widget kinds and geometry, and the per-session node and
resolved-control budgets. The old session is replaced only after the
prospective tree has validated and rendered successfully.

## Board and process budgets

The existing `RuntimeProfile.ui` has two independent compile-time capacities:

- `maxResolvedControls`: number of leaf controls stored by `GuestView`.
- `maxTreeNodes`: number of v3 tree entries, including non-rendering groups.

Valid configurations satisfy `8 <= maxResolvedControls <= maxTreeNodes <=
256`; v2 is still limited to its original 16 wire nodes. The defaults in
this increment are: host/simulator 32/32, Pico 12/16, PicoCalc Pico 2 W
16/16, PicoCalc Lyra 64/64. These are initial engineering limits, not
measured optimal SRAM budgets. Changing them changes the in-memory size
of `GuestView`; all translation units for one firmware must agree on the
same target profile.

UI service allocates the prospective `GuestView` temporarily on the
host heap to avoid placing a large resolved document on a small task
stack. The RV32I syscall adapter and native transport also copy v3
node records into temporary host-owned arrays; allocation failure
rejects the request without changing the current session.

## Application API

For a high-level non-owning object API:

```cpp
#include <lexx/ui.h>

lexx::ui::UiCapabilities caps{};
if (lexx::ui::capabilities(caps) != 0 || caps.protocolVersion < 3 ||
    caps.maxNodes < 22 || caps.maxControls < 20) {
    // Choose a smaller layout or an earlier protocol.
}

lexx::ui::TreeWindowV3<24> window{"Settings"};
lexx::ui::Column root{100};
lexx::ui::Button confirm{1, "Confirm"};
window.add(root);
window.add(confirm, &root);
// Register remaining live widgets, then:
window.exec();
```

For a low-level counted document, use
`lexx::ui::TreeDocumentV3<Capacity>` and `lexx::ui::show_tree_v3(doc)`.
Only `36 + doc.header.count * 52` bytes are passed to the VM. Native
applications use `Transport::query_capabilities` and
`Transport::show_tree_v3`.

`TreeWindowV3<Capacity>` is non-owning and keeps its descriptor storage
inside the instance. Large capacities require a sufficient native/guest
stack or placing the instance in longer-lived storage. ABI v1/v2 windows
and their applications continue using the original APIs.

## Scope

This is a **counted full-tree replacement**, not incremental mutations.
The initial v3 resolver retains the same four-level nesting limit,
single list-item table and full redraw behavior. Atomic subtree
patch/remove, clipping and list virtualization remain subsequent steps.

## Verification

Build `lexxos_ui_tree_tests` and run the host CTest suite. Tests cover
capability values, v3 object transport, 20-node resolution on large
profiles, focus persistence inside the newly shown window, validation
of an invalid version and duplicate IDs, and legacy v2 layout.
Repeat native and guest UI demos on the simulator and Lyra. Actual
firmware resource headroom must be measured separately.
