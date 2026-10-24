#pragma once

#include <cstdint>

#include <lexxos/devices/rtc/date_time.h>

namespace lexxos::devices {

class I2cBus;

namespace rtc {

	struct Pcf8563Status {
		bool voltageLow{false};
		std::uint8_t secondsRaw{0};
	};

	class Pcf8563Rtc {
	public:
		static constexpr std::uint8_t defaultAddress = 0x51;

		Pcf8563Rtc(I2cBus& bus, std::uint8_t address = defaultAddress);

		[[nodiscard]] bool probe();
		[[nodiscard]] bool readDateTime(DateTime& output);
		[[nodiscard]] bool writeDateTime(DateTime const& value);
		[[nodiscard]] bool readStatus(Pcf8563Status& output);

	private:
		[[nodiscard]] bool readRegisters(std::uint8_t firstRegister, std::uint8_t* data, std::uint8_t size);
		[[nodiscard]] bool writeRegisters(std::uint8_t firstRegister, std::uint8_t const* data, std::uint8_t size);

		I2cBus* _bus{nullptr};
		std::uint8_t _address{defaultAddress};
	};

} // namespace rtc
} // namespace lexxos::devices
