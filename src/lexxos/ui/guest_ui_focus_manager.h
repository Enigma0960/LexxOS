#pragma once

#include <cstdint>

#include <lexxos/ui/guest_ui_focus.h>

namespace lexxos::ui {

// Owns focus selection for both v1 and v2 resolved documents.
class FocusManager final {
public:
	[[nodiscard]] int index() const { return _index; }

	void reset(ResolvedDocument const& document) {
		_index = -1;
		for (std::uint16_t i = 0; i < document.count; ++i) {
			if (controls::canFocus(document.controls[i])) {
				_index = i;
				break;
			}
		}
	}

	[[nodiscard]] bool setIndex(ResolvedDocument const& document, int index) {
		if (index < 0 || index >= static_cast<int>(document.count) || !controls::canFocus(document.controls[index]) || _index == index) {
			return false;
		}

		_index = index;
		return true;
	}

	[[nodiscard]] bool setId(ResolvedDocument const& document, std::uint16_t id) {
		for (std::uint16_t i = 0; i < document.count; ++i) {
			if (document.controls[i].id == id) {
				return setIndex(document, i);
			}
		}

		return false;
	}

	[[nodiscard]] bool moveSequential(ResolvedDocument const& document, int step) {
		if (!document.count || step == 0) {
			return false;
		}

		if (_index < 0) {
			reset(document);
			return _index >= 0;
		}

		int next = _index;
		for (std::uint16_t attempt = 0; attempt < document.count; ++attempt) {
			next = (next + (step > 0 ? 1 : -1) + document.count) % document.count;
			if (controls::canFocus(document.controls[next])) {
				return setIndex(document, next);
			}
		}

		return false;
	}

	[[nodiscard]] bool moveSpatial(ResolvedDocument const& document, devices::KeyCode direction) {
		return setIndex(document, focus::spatialNeighbor(document, _index, direction));
	}

	[[nodiscard]] bool reconcile(ResolvedDocument const& document) {
		if (_index >= 0 && _index < static_cast<int>(document.count) && controls::canFocus(document.controls[_index])) {
			return false;
		}

		int const previous = _index;
		reset(document);
		return previous != _index;
	}

private:
	int _index{-1};
};

} // namespace lexxos::ui
