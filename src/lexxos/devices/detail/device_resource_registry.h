#pragma once

#include <atomic>
#include <cstddef>
#include <string_view>

#include <lexxos/devices/device_resource.h>

namespace lexxos::devices::detail {

inline constexpr std::size_t invalidDeviceResourceIndex = static_cast<std::size_t>(-1);

struct DeviceResourceState {
	std::atomic_bool locked{false};
	DeviceResourcePriority priority{DeviceResourcePriority::Normal};
	std::string_view owner{};
};

template <typename Descriptor, std::size_t Capacity>
class DeviceResourceRegistry {
public:
	void clear() {
		for (std::size_t index = 0; index < Capacity; ++index) {
			_descriptors[index] = {};
			resetState(_states[index]);
		}
		_count = 0;
	}

	[[nodiscard]] bool add(Descriptor descriptor) {
		if (descriptor.name.empty() || _count >= Capacity || findIndex(descriptor.name) != invalidDeviceResourceIndex) {
			return false;
		}

		_descriptors[_count] = descriptor;
		resetState(_states[_count]);
		++_count;
		return true;
	}

	[[nodiscard]] Descriptor* find(std::string_view name) {
		auto const index = findIndex(name);
		return index != invalidDeviceResourceIndex ? &_descriptors[index] : nullptr;
	}

	[[nodiscard]] Descriptor const* find(std::string_view name) const {
		auto const index = findIndex(name);
		return index != invalidDeviceResourceIndex ? &_descriptors[index] : nullptr;
	}

	[[nodiscard]] std::size_t findIndex(std::string_view name) const {
		for (std::size_t index = 0; index < _count; ++index) {
			if (_descriptors[index].name == name) {
				return index;
			}
		}
		return invalidDeviceResourceIndex;
	}

	[[nodiscard]] std::size_t count() const {
		return _count;
	}

	[[nodiscard]] Descriptor const& at(std::size_t index) const {
		return _descriptors[index];
	}

	[[nodiscard]] bool isLocked(std::string_view name) const {
		auto const index = findIndex(name);
		return index != invalidDeviceResourceIndex && isLockedAt(index);
	}

	[[nodiscard]] bool isLockedAt(std::size_t index) const {
		return statusAt(index).locked;
	}

	[[nodiscard]] DeviceResourceStatus statusAt(std::size_t index) const {
		if (index >= _count) {
			return {};
		}

		auto const locked = _states[index].locked.load(std::memory_order_acquire);
		return {
		    .locked = locked,
		    .priority = locked ? _states[index].priority : DeviceResourcePriority::Normal,
		    .owner = locked ? _states[index].owner : std::string_view{},
		};
	}

	[[nodiscard]] bool tryLock(std::size_t index, DeviceResourcePriority priority, std::string_view owner) {
		if (index >= _count) {
			return false;
		}

		bool expected = false;
		if (!_states[index].locked.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
			return false;
		}

		_states[index].priority = priority;
		_states[index].owner = owner;
		return true;
	}

	void release(std::size_t index) {
		if (index >= _count) {
			return;
		}

		resetState(_states[index]);
	}

private:
	static void resetState(DeviceResourceState& state) {
		state.owner = {};
		state.priority = DeviceResourcePriority::Normal;
		state.locked.store(false, std::memory_order_release);
	}

	Descriptor _descriptors[Capacity]{};
	DeviceResourceState _states[Capacity]{};
	std::size_t _count{0};
};

} // namespace lexxos::devices::detail
