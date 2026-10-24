#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/infrared_transmitter.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

class CardputerIrTransmitter final : public InfraredTransmitter {
public:
	CardputerIrTransmitter(
	    lexxos::platform::InfraredTransmitterPins config,
	    lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] std::uint32_t carrierFrequencyHz() const override;
	[[nodiscard]] int outputPinNumber() const override;
	bool transmitSamsung(std::uint32_t code, std::uint8_t repeats) override;

private:
	[[nodiscard]] bool _initializeRmt();
	[[nodiscard]] bool _sendSamsungFrame(std::uint32_t code) const;

	lexxos::platform::InfraredTransmitterPins _config{};
	lexxos::platform::PlatformServices const* _platform{nullptr};
	void* _rmtChannel{nullptr};
	void* _rmtEncoder{nullptr};
	bool _ready{false};
};

} // namespace lexxos::devices
