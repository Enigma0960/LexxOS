#pragma once

#include <array>
#include <cstddef>

#include <lexxos/devices/input_device.h>

namespace lexxos::devices::input_detail {

template <std::size_t Capacity>
class InputEventQueue final {
	static_assert(Capacity > 0);

public:
	static constexpr std::size_t capacity = Capacity;

	void clear() {
		_head = 0;
		_tail = 0;
		_count = 0;
	}

	[[nodiscard]] bool empty() const {
		return _count == 0;
	}

	[[nodiscard]] bool full() const {
		return _count >= _events.size();
	}

	[[nodiscard]] std::size_t size() const {
		return _count;
	}

	void push(InputEvent const& event) {
		if (full()) {
			_tail = (_tail + 1) % _events.size();
			--_count;
		}

		_events[_head] = event;
		_head = (_head + 1) % _events.size();
		++_count;
	}

	[[nodiscard]] bool pop(InputEvent& event) {
		if (empty()) {
			return false;
		}

		event = _events[_tail];
		_tail = (_tail + 1) % _events.size();
		--_count;
		return true;
	}

private:
	std::array<InputEvent, Capacity> _events{};
	std::size_t _head{0};
	std::size_t _tail{0};
	std::size_t _count{0};
};

} // namespace lexxos::devices::input_detail
