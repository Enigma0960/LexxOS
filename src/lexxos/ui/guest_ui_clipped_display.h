#pragma once

#include <algorithm>
#include <cstdint>

#include <lexxos/devices/display_device.h>

namespace lexxos::ui {

// A pixel-accurate clipping adapter. Coordinates passed by controls remain
// global; only the actual write to the backing display is cropped.
class ClippedDisplay final : public devices::DisplayDevice {
public:
	ClippedDisplay(devices::DisplayDevice& target, devices::Rect clip)
	    : _target(target)
	    , _clip(clip) {}

	[[nodiscard]] std::string_view name() const override { return _target.name(); }

	[[nodiscard]] bool isReady() const override { return _target.isReady(); }

	[[nodiscard]] int width() const override { return _target.width(); }

	[[nodiscard]] int height() const override { return _target.height(); }

	[[nodiscard]] devices::PixelFormat nativeFormat() const override { return _target.nativeFormat(); }

	[[nodiscard]] devices::DisplayWireFormat wireFormat() const override { return _target.wireFormat(); }

	bool clear(std::uint16_t color) override { return fillRgb565({0, 0, width(), height()}, color); }

	bool beginFrame() override { return true; } // Caller owns the actual frame.

	bool endFrame() override { return true; }

	bool fillRgb565(devices::Rect const& area, std::uint16_t color) override {
		devices::Rect clipped{};
		return !intersection(area, clipped) || _target.fillRgb565(clipped, color);
	}

	bool blitRgb565(devices::Rect const& area, std::uint16_t const* pixels, int stride) override {
		devices::Rect clipped{};
		if (!intersection(area, clipped)) {
			return true;
		}

		if (pixels == nullptr || stride < area.width) {
			return false;
		}

		pixels += (clipped.y - area.y) * stride + (clipped.x - area.x);
		return _target.blitRgb565(clipped, pixels, stride);
	}

	bool blitRgb565Alpha(devices::Rect const& area, std::uint16_t const* pixels,
	    std::uint8_t const* alpha, int stride) override {
		devices::Rect clipped{};
		if (!intersection(area, clipped)) {
			return true;
		}

		if (pixels == nullptr || alpha == nullptr || stride < area.width) {
			return false;
		}

		int const offset = (clipped.y - area.y) * stride + clipped.x - area.x;
		return _target.blitRgb565Alpha(clipped, pixels + offset, alpha + offset, stride);
	}

private:
	bool intersection(devices::Rect const& area, devices::Rect& result) const {
		if (area.width <= 0 || area.height <= 0 || _clip.width <= 0 || _clip.height <= 0) {
			return false;
		}

		int const left = std::max({area.x, _clip.x, 0});
		int const top = std::max({area.y, _clip.y, 0});
		int const right = std::min({area.x + area.width, _clip.x + _clip.width, width()});
		int const bottom = std::min({area.y + area.height, _clip.y + _clip.height, height()});
		if (right <= left || bottom <= top) {
			return false;
		}

		result = {left, top, right - left, bottom - top};
		return true;
	}

	devices::DisplayDevice& _target;
	devices::Rect _clip;
};

} // namespace lexxos::ui
