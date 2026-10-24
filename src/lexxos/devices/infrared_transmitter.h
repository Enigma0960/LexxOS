#pragma once

#include <cstdint>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

class InfraredTransmitter : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override {
		return DeviceKind::Infrared;
	}

	[[nodiscard]] virtual std::uint32_t carrierFrequencyHz() const = 0;
	[[nodiscard]] virtual int outputPinNumber() const = 0;
	virtual bool transmitSamsung(std::uint32_t code, std::uint8_t repeats) = 0;
};

} // namespace lexxos::devices
