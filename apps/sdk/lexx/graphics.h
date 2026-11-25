#pragma once

#include <lexx/lexx.h>

namespace lexx::graphics {

// All rendering is performed by the host. The guest never owns a framebuffer.
// Result: 0=OK, -1=invalid, -2=no display, -3=busy, -4=denied,
// -5=not ready, -6=unsupported, -7=I/O error.
inline constexpr int ok = 0;
inline constexpr int denied = -4;

enum class Mode : uint32_t {
	Text = 0,
	Graphics = 1,
};

enum class CommandType : uint32_t {
	FillRect = 1,
	Clear = 2,
	BlitImage = 3,
	Text = 4,
	OutlineRect = 5,
	Pixel = 6,
	BlitRegion = 7, // image atlas; dimensions packed in height, source xy in color
	TextTransparent = 8, // same text layout, but preserve pixels behind the glyph
	BlitImageScaled = 9, // width=handle, height=scale (1/2/4/8/16)
	BlitRegionScaled2 = 10, // same arguments as BlitRegion, nearest-neighbor x2
	BlitRegionScaled4 = 11, // nearest-neighbor x4
	BlitRegionScaled8 = 12, // nearest-neighbor x8
	BlitRegionScaled16 = 13, // nearest-neighbor x16
};

struct DisplayInfo {
	uint16_t width{0};
	uint16_t height{0};
	uint16_t pixel_format{0}; // 1 = RGB565
	uint16_t reserved{0};
};

// ABI: 20 bytes, returned by graphics operation 17.
struct ImageLimits {
	uint32_t max_handles{0};
	uint32_t max_dimension{0};
	uint32_t max_pixels{0};
	uint32_t cache_bytes{0};
	uint32_t stream_chunk_bytes{0};
};

static_assert(sizeof(ImageLimits) == 20);

// Fixed-width, little-endian RV32I ABI. Maximum 64 commands per submit.
// FillRect/OutlineRect: width/height are dimensions, color=RGB565.
// BlitImage: width=image handle, height/color unused; clipped to screen.
// BlitRegion: width=handle, height=(srcHeight<<16)|srcWidth,
//             color=(srcY<<16)|srcX, x/y=destination; alpha8 if the resource has it.
// Text: width=(uint32_t)zero-terminated guest string, height=scale (1 or 2),
//       low 16 bits of color=foreground RGB565, high 16 bits=background.
// TextTransparent: same width/height, low 16 bits=foreground; background untouched.
// Pixel: x/y are coordinates, low 16 bits of color=RGB565.
struct Command {
	uint32_t type{0};
	int32_t x{0};
	int32_t y{0};
	int32_t width{0};
	int32_t height{0};
	uint32_t color{0};
};

static_assert(sizeof(DisplayInfo) == 8);
static_assert(sizeof(Command) == 24);

// Pack two RGB565 colors into the existing Text command ABI.
[[nodiscard]] constexpr uint32_t textColor(uint16_t foreground, uint16_t background) {
	return (static_cast<uint32_t>(background) << 16U) | foreground;
}

static_assert(textColor(0xFFFFU, 0x1234U) == 0x1234FFFFU);

// Transparent text does not erase the underlying pixels. The caller must redraw
// the previous text when updating a string on a persistent framebuffer.
[[nodiscard]] inline Command transparentText(int32_t x, int32_t y,
    char const* text, int32_t scale, uint16_t foreground) {
	return {static_cast<uint32_t>(CommandType::TextTransparent), x, y,
	    static_cast<int32_t>(reinterpret_cast<uint32_t>(text)), scale, foreground};
}

// Atlas source rectangles are unsigned 16-bit; guest destination may be signed.
[[nodiscard]] constexpr Command sprite_region(uint32_t handle, int32_t x, int32_t y,
    uint32_t sx, uint32_t sy, uint32_t w, uint32_t h) {
	return {static_cast<uint32_t>(CommandType::BlitRegion), x, y,
	    static_cast<int32_t>(handle), static_cast<int32_t>((h << 16U) | w),
	    (sy << 16U) | sx};
}

// Host-side nearest-neighbor scaling; no expanded guest image buffer.
// The OS must implement command types 9..13 before these helpers are usable.
[[nodiscard]] constexpr bool valid_sprite_scale(uint32_t scale) {
	return scale == 1U || scale == 2U || scale == 4U || scale == 8U || scale == 16U;
}

[[nodiscard]] constexpr Command sprite_image_scaled(uint32_t handle,
    int32_t x, int32_t y, uint32_t scale = 1U) {
	return scale == 1U
	    ? Command{static_cast<uint32_t>(CommandType::BlitImage), x, y,
	          static_cast<int32_t>(handle), 0, 0}
	    : Command{static_cast<uint32_t>(CommandType::BlitImageScaled), x, y,
	          static_cast<int32_t>(handle), static_cast<int32_t>(scale), 0};
}

[[nodiscard]] constexpr Command sprite_region_scaled(uint32_t handle,
    int32_t x, int32_t y, uint32_t sx, uint32_t sy,
    uint32_t w, uint32_t h, uint32_t scale = 1U) {
	uint32_t const type = scale == 2U ? static_cast<uint32_t>(CommandType::BlitRegionScaled2)
	    : scale == 4U                 ? static_cast<uint32_t>(CommandType::BlitRegionScaled4)
	    : scale == 8U                 ? static_cast<uint32_t>(CommandType::BlitRegionScaled8)
	    : scale == 16U                ? static_cast<uint32_t>(CommandType::BlitRegionScaled16)
	                                  : static_cast<uint32_t>(CommandType::BlitRegion);
	return {type, x, y, static_cast<int32_t>(handle),
	    static_cast<int32_t>((h << 16U) | w), (sy << 16U) | sx};
}

enum class AnimationMode : uint32_t {
	Loop,
	PingPong
};

// Pure guest-side frame selection. Does not allocate and has no host dependency.
struct SpriteAnimation {
	uint32_t frame_count{1};
	uint32_t frame_ms{100};
	AnimationMode mode{AnimationMode::Loop};

	[[nodiscard]] constexpr uint32_t frame_at(uint32_t elapsed_ms) const {
		if (frame_count <= 1U || frame_ms == 0U) {
			return 0;
		}

		uint32_t const step = elapsed_ms / frame_ms;
		if (mode == AnimationMode::Loop) {
			return step % frame_count;
		}

		uint32_t const period = 2U * (frame_count - 1U);
		uint32_t const p = step % period;
		return p < frame_count ? p : period - p;
	}
};

inline int get_info(DisplayInfo& info) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 0U,
	    reinterpret_cast<uint32_t>(&info), 0U));
}

inline int set_mode(Mode mode) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 1U,
	    static_cast<uint32_t>(mode), 0U));
}

inline int submit(Command const* commands, uint32_t count) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 2U,
	    reinterpret_cast<uint32_t>(commands), count));
}

// Load a resource into the bounded system-owned image cache. Image dimensions,
// cache budget and available handle count are supplied by the board profile.
inline int load_image(char const* resource_path) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 3U,
	    reinterpret_cast<uint32_t>(resource_path), 0U));
}

// Metadata-only handle; draws requested rows from the embedded :/ resource.
// Avoid on per-frame sprites when SD access is slow: prefer load_image().
inline int load_image_streamed(char const* resource_path) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 3U,
	    reinterpret_cast<uint32_t>(resource_path), 1U));
}

inline int get_limits(ImageLimits& limits) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 17U,
	    reinterpret_cast<uint32_t>(&limits), 0U));
}

inline int unload_image(uint32_t handle) {
	return static_cast<int>(detail::ecall3(syscall_graphics, 4U, handle, 0U));
}

// Wait for the last submitted frame to reach the display presenter.
// The host emulator has no physical VSYNC; this is not a VSYNC fence.
inline int wait_idle() {
	return static_cast<int>(detail::ecall3(syscall_graphics, 5U, 0U, 0U));
}

} // namespace lexx::graphics
