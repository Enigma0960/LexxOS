#include <lexxos/devices/display/dirty_region_set.h>

namespace lexxos::devices {

void DirtyRegionSet::clear() {
	_count = 0;
}

bool DirtyRegionSet::empty() const {
	return _count == 0;
}

std::size_t DirtyRegionSet::size() const {
	return _count;
}

std::uint64_t DirtyRegionSet::pixels() const {
	std::uint64_t result = 0;
	for (std::size_t i = 0; i < _count; ++i) {
		result += area(_regions[i]);
	}

	return result;
}

void DirtyRegionSet::add(Rect rect) {
	if (rect.width <= 0 || rect.height <= 0) {
		return;
	}

	_regions[_count++] = rect;
	coalesceTouches();
	if (_count > maxRegions) {
		// Minimize additional transmitted area, not just rectangle count.
		// A full-screen bounding box is a poor fallback for a small HUD
		// and several widely separated sprites.
		std::size_t bestA = 0;
		std::size_t bestB = 1;
		std::int64_t bestCost = mergeCost(_regions[0], _regions[1]);
		for (std::size_t i = 0; i < _count; ++i) {
			for (std::size_t j = i + 1; j < _count; ++j) {
				auto const cost = mergeCost(_regions[i], _regions[j]);
				if (cost < bestCost) {
					bestA = i;
					bestB = j;
					bestCost = cost;
				}
			}
		}

		_regions[bestA] = bounding(_regions[bestA], _regions[bestB]);
		erase(bestB);
		coalesceTouches();
	}
}

std::uint64_t DirtyRegionSet::area(Rect const& rect) {
	return static_cast<std::uint64_t>(rect.width) * static_cast<std::uint64_t>(rect.height);
}

Rect DirtyRegionSet::bounding(Rect const& a, Rect const& b) {
	int const left = a.x < b.x ? a.x : b.x;
	int const top = a.y < b.y ? a.y : b.y;
	int const right = a.x + a.width > b.x + b.width ? a.x + a.width : b.x + b.width;
	int const bottom = a.y + a.height > b.y + b.height ? a.y + a.height : b.y + b.height;
	return {left, top, right - left, bottom - top};
}

bool DirtyRegionSet::touches(Rect const& a, Rect const& b) {
	return a.x <= b.x + b.width && b.x <= a.x + a.width && a.y <= b.y + b.height && b.y <= a.y + a.height;
}

std::int64_t DirtyRegionSet::mergeCost(Rect const& a, Rect const& b) {
	return static_cast<std::int64_t>(area(bounding(a, b)))
	    - static_cast<std::int64_t>(area(a))
	    - static_cast<std::int64_t>(area(b));
}

void DirtyRegionSet::erase(std::size_t index) {
	--_count;
	if (index != _count) {
		_regions[index] = _regions[_count];
	}
}

void DirtyRegionSet::coalesceTouches() {
	bool again = true;
	while (again) {
		again = false;
		for (std::size_t i = 0; i < _count && !again; ++i) {
			for (std::size_t j = i + 1; j < _count; ++j) {
				if (touches(_regions[i], _regions[j])) {
					_regions[i] = bounding(_regions[i], _regions[j]);
					erase(j);
					again = true;
					break;
				}
			}
		}
	}
}

} // namespace lexxos::devices
