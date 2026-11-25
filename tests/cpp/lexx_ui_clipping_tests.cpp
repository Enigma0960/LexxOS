#include <cassert>
#include <cstdint>
#include <string_view>

#include <lexxos/ui/guest_ui_clipped_display.h>

namespace {

class RecordingDisplay final : public lexxos::devices::DisplayDevice {
public:
	[[nodiscard]] std::string_view name() const override { return "recording"; }

	[[nodiscard]] bool isReady() const override { return true; }

	[[nodiscard]] int width() const override { return 32; }

	[[nodiscard]] int height() const override { return 32; }

	[[nodiscard]] lexxos::devices::PixelFormat nativeFormat() const override {
		return lexxos::devices::PixelFormat::Rgb565;
	}

	bool clear(std::uint16_t) override { return true; }

	[[nodiscard]] bool supportsPartialUpdates() const override { return true; }

	bool beginFrame() override {
		++frames;
		return true;
	}

	bool endFrame() override {
		++ends;
		return true;
	}

	bool fillRgb565(lexxos::devices::Rect const& rect, std::uint16_t) override {
		last = rect;
		++writes;
		return true;
	}

	bool blitRgb565(lexxos::devices::Rect const& rect,
	    std::uint16_t const* pixels, int stride) override {
		last = rect;
		first = *pixels;
		lastStride = stride;
		++writes;
		return true;
	}

	lexxos::devices::Rect last{};
	int frames{0}, ends{0}, writes{0}, lastStride{0};
	std::uint16_t first{0};
};

} // namespace

int main() {
	RecordingDisplay display{};
	lexxos::ui::ClippedDisplay clipped{
	    display, {5, 5, 10, 10}
    };
	assert(clipped.fillRgb565({0, 0, 8, 8}, 0));
	assert(display.writes == 1 && display.last.x == 5 && display.last.y == 5);
	assert(display.last.width == 3 && display.last.height == 3);
	assert(clipped.fillRgb565({20, 20, 5, 5}, 0) && display.writes == 1);
	std::uint16_t pixels[100]{};
	for (int i = 0; i < 100; ++i) {
		pixels[i] = static_cast<std::uint16_t>(i);
	}

	assert(clipped.blitRgb565({0, 0, 10, 10}, pixels, 10));
	assert(display.writes == 2 && display.first == 55 && display.lastStride == 10);
	assert(display.last.width == 5 && display.last.height == 5);
	assert(clipped.beginFrame() && clipped.endFrame());
	assert(display.frames == 0 && display.ends == 0);
}
