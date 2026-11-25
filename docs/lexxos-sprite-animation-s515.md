# LexxOS S5.15 — bounded sprite animation SDK

`apps/sdk/lexx/animation_timeline.h` supplies a deterministic, heap-free
animation clock. A clip supports fixed or per-frame durations (1..64 frames),
loop, ping-pong and once playback, and pause/resume. Unsigned 32-bit time
subtraction handles monotonic `milliseconds()` wraparound for normal animation
intervals (do not expect accurate playback after a full 49.7-day clock wrap).

`apps/sdk/lexx/sprite_player.h` maps frames onto a regular LXIM grid and
builds at most two graphics commands per changed frame: restore old region
with a solid RGB565 color, then `BlitRegion` the new frame. The host owns
all image pixels and alpha8 blending. No guest framebuffer is required.

## Example

```cpp
#include <lexx/sprite_player.h>

int handle = lexx::graphics::load_image(":/img/orb.lxim");
lexx::graphics::SpriteSheet sheet{
    static_cast<unsigned>(handle), 168, 24, 24, 24};
lexx::graphics::AnimationClip clip{
    0U, 7U, 90U, lexx::graphics::PlaybackMode::Loop};
lexx::graphics::SpritePlayer player{sheet, clip};
player.start(lexx::milliseconds());
// In a foreground graphics session:
lexx::graphics::Command commands[2]{};
unsigned count = 0;
if (player.append(commands, 2U, count, x, y,
        lexx::milliseconds(), 0x18E3U) && count != 0U) {
    if (lexx::graphics::submit(commands, count) == 0) {
        player.committed();
    } else {
        player.cancelled();
    }
}
```

The supplied LXIM resource must match the sheet dimensions and have a
regular atlas grid. The caller owns the image handle, loads it once, and
unloads it after the animation is finished. `start()` resets dirty history.

**Limitation:** dirty restoration uses a single solid background color.
For scenes with image backgrounds, redraw the affected background from
its own atlas/scene before the sprite blit; do not rely on `FillRect`.
There is no automatic layering, collision, texture decode, or sprite pack
streaming yet. The graphics backend currently offers two cached LXIM image
handles and 8192-pixel image limits. Next work should convert a sprite
clip into a bounded LXIM sheet and benchmark host cache and SPI traffic.

Use the existing `sdk-demo.lexx` alpha-sprite test, rather than creating
another diagnostic application. Host-side timeline unit tests are registered
as `lexx_animation_timeline_tests` in the tests CMake file.
