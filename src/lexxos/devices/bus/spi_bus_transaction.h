#pragma once

#include <cstdint>

#include <lexxos/devices/device_resource.h>

namespace lexxos::devices {

class SpiBus;

class SpiBusTransaction {
public:
	explicit SpiBusTransaction(SpiBus& bus,
	    DeviceResourcePriority priority = DeviceResourcePriority::System,
	    std::uint32_t timeoutMs = 100);

	SpiBusTransaction(SpiBusTransaction const&) = delete;
	SpiBusTransaction& operator=(SpiBusTransaction const&) = delete;

	SpiBusTransaction(SpiBusTransaction&& other) noexcept;

	SpiBusTransaction& operator=(SpiBusTransaction&& other) noexcept;

	~SpiBusTransaction();

	[[nodiscard]] bool active() const;

	[[nodiscard]] explicit operator bool() const;

	void release();

private:
	SpiBus* _bus{nullptr};
	bool _active{false};
};

} // namespace lexxos::devices
