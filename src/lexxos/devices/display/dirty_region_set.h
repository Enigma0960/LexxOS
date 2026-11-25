#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/display_device.h>

namespace lexxos::devices {

// Bounded dirty-region accumulator. The extra slot permits selecting the
// least expensive pair to merge when a fifth independent rectangle arrives.
// No allocations or platform-specific code; rectangles must be valid and
// positive-sized when passed to add().
class DirtyRegionSet final {
public:
	static constexpr std::size_t maxRegions = 4;

	void clear();

	[[nodiscard]] bool empty() const;

	[[nodiscard]] std::size_t size() const;

	[[nodiscard]] constexpr Rect const& operator[](std::size_t index) const { return _regions[index]; }

	[[nodiscard]] std::uint64_t pixels() const;

	void add(Rect rect);

private:
	[[nodiscard]] static std::uint64_t area(Rect const& rect);

	[[nodiscard]] static Rect bounding(Rect const& a, Rect const& b);

	[[nodiscard]] static bool touches(Rect const& a, Rect const& b);

	[[nodiscard]] static std::int64_t mergeCost(Rect const& a, Rect const& b);

	void erase(std::size_t index);

	void coalesceTouches();

	std::array<Rect, maxRegions + 1> _regions{};
	std::size_t _count{0};
};

} // namespace lexxos::devices
