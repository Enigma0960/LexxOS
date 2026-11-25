#include <lexxos/devices/device_resource_lock.h>

#include <utility>

#include <lexxos/devices/device_manager.h>

namespace lexxos::devices {

DeviceResourceLock::DeviceResourceLock(DeviceManager& manager, Kind kind, std::size_t index)
    : _manager(&manager)
    , _kind(kind)
    , _index(index) {
}

DeviceResourceLock::DeviceResourceLock(DeviceResourceLock&& other) noexcept
    : _manager(std::exchange(other._manager, nullptr))
    , _kind(std::exchange(other._kind, Kind::None))
    , _index(std::exchange(other._index, 0)) {
}

DeviceResourceLock::~DeviceResourceLock() {
	release();
}

DeviceResourceLock::operator bool() const {
	return valid();
}

DeviceResourceLock& DeviceResourceLock::operator=(DeviceResourceLock&& other) noexcept {
	if (this != &other) {
		release();
		_manager = std::exchange(other._manager, nullptr);
		_kind = std::exchange(other._kind, Kind::None);
		_index = std::exchange(other._index, 0);
	}

	return *this;
}

bool DeviceResourceLock::valid() const {
	return _manager != nullptr && _kind != Kind::None;
}

void DeviceResourceLock::release() {
	if (_manager == nullptr || _kind == Kind::None) {
		return;
	}

	_manager->_release(_kind, _index);
	_manager = nullptr;
	_kind = Kind::None;
	_index = 0;
}

} // namespace lexxos::devices
