#pragma once

namespace lexxos::devices::detail {

class DeviceRegistryCriticalSection {
public:
	DeviceRegistryCriticalSection();
	~DeviceRegistryCriticalSection();
};

} // namespace lexxos::devices::detail
