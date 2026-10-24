#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/display/text_display.h>

namespace lexxos::devices {

class Ssd1306TextDisplay final : public TextDisplay {
public:
	static constexpr std::uint8_t defaultAddress = 0x3C;

	Ssd1306TextDisplay(I2cBus& bus, std::uint8_t address = defaultAddress);

	bool initialize() override;
	[[nodiscard]] bool isReady() const override;
	void clear() override;
	void writeLine(std::uint8_t line, std::string_view text) override;

private:
	bool write(std::uint8_t control, std::uint8_t const* data, std::size_t size);
	bool command(std::uint8_t command);
	void setPage(std::uint8_t page, std::uint8_t column);

	I2cBus& _bus;
	std::uint8_t _address{defaultAddress};
	bool _ready{false};
};

} // namespace lexxos::devices
