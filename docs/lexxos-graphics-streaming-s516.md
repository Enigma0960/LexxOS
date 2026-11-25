# LexxOS Graphics S5.16 — configurable image cache and resource streaming

Two public load modes use the existing LXIM v1 format (RGB565 and optional
planar alpha8), embedded resources `:/...` and existing graphics handles.
No raw framebuffer address is exposed to the guest.

## Board/profile limits

The `runtime::GraphicsRuntimeLimits` fields are configured through the
same chip/platform/board overlays as the other LexxOS budgets:

| Profile | Handles/VM | Maximum dimension | Pixels/image | Cache/VM | Stream scratch |
|---|---:|---:|---:|---:|---:|
| Base/host | 8 | 512 | 131072 | 192 KiB | 1024 B |
| Pico SDK | 3 | 256 | 32768 | 24 KiB | 512 B |
| PicoCalc Lyra | 12 | 512 | 262144 | 192 KiB | 1024 B |

These limits are compile-time firmware settings, **not allocations**. The
cache budget is enforced per guest VM and is separate from guest memory.
Profile limits do not change the `.lexx` manifest memory request. Total
host RAM is further constrained by the real heap/allocator.

Query the firmware with `lexx::graphics::get_limits(ImageLimits&)` (graphics
operation 17, 20-byte LE structure). Old code can continue calling
`load_image()` (graphics op 3, final argument 0). The new
`load_image_streamed()` uses op 3 with argument 1. Both return the same
positive handle range and are unloaded with `unload_image(handle)`.

## Cached and streamed images

`Cached`: read and verify the entire resource once; store RGB565/alpha8 in
host RAM and render without VFS I/O. The cumulative padded allocation size
of all cached images in one VM may not exceed `imageCacheBytes`. Loading
beyond this returns `-8` (no capacity). `unload_image` reclaims bytes.

`Streamed`: validate header and CRC in chunks at open time; retain only the
resource's absolute offset, dimensions and alpha flag. When rendering,
open the package and read the visible source row chunks using a bounded
scratch buffer. The same path supports opaque and alpha8 images. No guest
framebuffer or full host image allocation is needed. The source stays in
its original `.lexx` package and must remain available while drawing.

For animation, **do not stream repeatedly from slow SD** if the atlas fits
cache: each draw can involve multiple filesystem seeks and synchronous
reads. Consider a small cached active animation plus streamed backgrounds.
If storage is removed, rendering streamed images returns an I/O error.

The authoring tool permits resources up to 1024×1024; firmware limits may
be lower. A file larger than profile limits must be split at build time.
LXIM CRC covers both pixel and alpha planes. Index integrity is checked by
the existing Lexx resource loader; paths are still restricted to `:/`.

## Example

```cpp
lexx::graphics::ImageLimits caps{};
if (lexx::graphics::get_limits(caps) == 0) {
    // Choose based on expected FPS and available cache size.
}
int background = lexx::graphics::load_image_streamed(":/img/background.lxim");
int character = lexx::graphics::load_image(":/img/pet-idle.lxim");
// Both handles work with BlitImage and sprite_region().
// Always unload both at the end of the session.
```

## Test matrix

- LXIM header validation at old and expanded limits; incremental CRC.
- Cached image loading/unloading up to cumulative byte and handle caps.
- Alpha8 streamed region comparison with cached rendering in simulator.
- Repeated animation while a large background is streamed; measure VFS seeks,
  SD read time, present bytes and FPS on RK3506 Lyra.
- Verify failure when resource disappears and cancellation/VM cleanup.
- Build `apps` with the RISC-V toolchain, not only native unit tests.

## Follow-up performance work

Introduce a small row/stripe LRU cache (preferably per VM, with fixed memory
budget), async prefetch and a richer sprite-pack for PMD/TPK2 conversions.
The first streaming implementation performs synchronous VFS reads; it is
memory-bounded but is not a guarantee of a particular FPS.
