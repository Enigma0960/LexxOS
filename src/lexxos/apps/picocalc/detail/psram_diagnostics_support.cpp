#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/gpio.h>
	#include <pico/platform.h>
	#include <pico/time.h>
#endif

namespace lexxos::apps::picocalc_programs::detail {
namespace {
	constexpr std::uint8_t psramJedecIdCommand = 0x9F;

	bool parseUnsignedFlexible(std::string_view text, std::uint32_t& output) {
		if (text.empty()) {
			return false;
		}

		std::uint32_t base = 10;
		std::size_t index = 0;
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			base = 16;
			index = 2;
			if (index >= text.size()) {
				return false;
			}
		}

		std::uint32_t value = 0;
		for (; index < text.size(); ++index) {
			char const character = text[index];
			std::uint32_t digit = 0;
			if (character >= '0' && character <= '9') {
				digit = static_cast<std::uint32_t>(character - '0');
			} else if (character >= 'a' && character <= 'f') {
				digit = static_cast<std::uint32_t>(10 + character - 'a');
			} else if (character >= 'A' && character <= 'F') {
				digit = static_cast<std::uint32_t>(10 + character - 'A');
			} else {
				return false;
			}
			if (digit >= base || value > (std::numeric_limits<std::uint32_t>::max() - digit) / base) {
				return false;
			}
			value = value * base + digit;
		}

		output = value;
		return true;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	uint picoGpio(lexxos::platform::GpioPin pin) {
		return static_cast<uint>(pin.number);
	}

	void configurePsramOutput(lexxos::platform::GpioPin pin, bool level) {
		if (!pin.isValid()) {
			return;
		}
		auto const gpio = picoGpio(pin);
		gpio_init(gpio);
		gpio_set_function(gpio, GPIO_FUNC_SIO);
		gpio_set_dir(gpio, GPIO_OUT);
		gpio_put(gpio, level ? 1 : 0);
	}

	bool configurePsramSpiPins(lexxos::platform::PsramPins const& pins) {
		if (!pins.spi.isValid()) {
			return false;
		}

		configurePsramOutput(pins.spi.cs, true);
		configurePsramOutput(pins.spi.sck, false);
		configurePsramOutput(pins.spi.mosi, false);
		configurePsramOutput(pins.sio2, true);
		configurePsramOutput(pins.sio3, true);
		auto const miso = picoGpio(pins.spi.miso);
		gpio_init(miso);
		gpio_set_function(miso, GPIO_FUNC_SIO);
		gpio_set_dir(miso, GPIO_IN);
		gpio_disable_pulls(miso);
		return true;
	}

	void psramBitDelay(std::uint32_t baudRate) {
		std::uint32_t const delayUs = baudRate == 0 ? 1 : (500000U + baudRate - 1U) / baudRate;
		busy_wait_us_32(delayUs == 0 ? 1 : delayUs);
	}

	std::uint8_t transferPsramByte(lexxos::platform::PsramPins const& pins, std::uint8_t value) {
		std::uint8_t result = 0;
		for (std::uint8_t bit = 0; bit < 8; ++bit) {
			gpio_put(picoGpio(pins.spi.mosi), (value & 0x80U) != 0 ? 1 : 0);
			psramBitDelay(pins.spi.baudRate);
			gpio_put(picoGpio(pins.spi.sck), 1);
			psramBitDelay(pins.spi.baudRate);
			result = static_cast<std::uint8_t>(
			    (result << 1U) | (gpio_get(picoGpio(pins.spi.miso)) != 0 ? 1U : 0U));
			gpio_put(picoGpio(pins.spi.sck), 0);
			psramBitDelay(pins.spi.baudRate);
			value = static_cast<std::uint8_t>(value << 1U);
		}
		return result;
	}

	void selectPsram(lexxos::platform::PsramPins const& pins) {
		psramBitDelay(pins.spi.baudRate);
		gpio_put(picoGpio(pins.spi.cs), 0);
		psramBitDelay(pins.spi.baudRate);
	}

	void deselectPsram(lexxos::platform::PsramPins const& pins) {
		gpio_put(picoGpio(pins.spi.cs), 1);
		psramBitDelay(pins.spi.baudRate);
	}

	void transferPsramAddress(lexxos::platform::PsramPins const& pins, std::uint32_t address) {
		(void) transferPsramByte(pins, static_cast<std::uint8_t>((address >> 16U) & 0xFFU));
		(void) transferPsramByte(pins, static_cast<std::uint8_t>((address >> 8U) & 0xFFU));
		(void) transferPsramByte(pins, static_cast<std::uint8_t>(address & 0xFFU));
	}

	std::uint8_t psramPatternByte(std::uint32_t address, std::size_t offset, std::uint8_t seed) {
		return static_cast<std::uint8_t>((address + static_cast<std::uint32_t>(offset)) ^ seed);
	}

#endif

} // namespace

bool parseOptionalUnsignedFlexible(
    lexxos::program::ParsedArguments const& arguments,
    std::string_view name,
    std::uint32_t& value) {
	auto const text = arguments.value(name);
	if (text.empty()) {
		return true;
	}
	return parseUnsignedFlexible(text, value);
}

void writeHexAddress(lexxos::os::AppContext& context, std::uint32_t value) {
	context.write("0x");
	writeHex32(context, value);
}

bool readPsramJedecId(
    lexxos::platform::BoardDescriptor const& board,
    std::array<std::uint8_t, 8>& output) {
#if defined(LEXXOS_PLATFORM_PICO)
	auto const& pins = board.psram;
	if (!configurePsramSpiPins(pins)) {
		return false;
	}

	selectPsram(pins);
	(void) transferPsramByte(pins, psramJedecIdCommand);
	for (std::size_t index = 0; index < output.size(); ++index) {
		output[index] = transferPsramByte(pins, 0xFF);
	}
	deselectPsram(pins);
	return true;
#else
	(void) board;
	(void) output;
	return false;
#endif
}

bool readPsramMemory(
    lexxos::platform::PsramPins const& pins,
    std::uint32_t address,
    std::uint8_t* output,
    std::size_t length) {
#if defined(LEXXOS_PLATFORM_PICO)
	if (output == nullptr || length == 0 || !pins.spi.isValid()) {
		return false;
	}

	(void) pins;
	return lexxos::platform::readGameBoyRomPsram(address, output, length);
#else
	(void) pins;
	(void) address;
	(void) output;
	(void) length;
	return false;
#endif
}

bool writePsramMemory(
    lexxos::platform::PsramPins const& pins,
    std::uint32_t address,
    std::uint8_t const* data,
    std::size_t length) {
#if defined(LEXXOS_PLATFORM_PICO)
	if (data == nullptr || length == 0 || !pins.spi.isValid()) {
		return false;
	}

	(void) pins;
	return lexxos::platform::writeGameBoyRomPsram(address, data, length);
#else
	(void) pins;
	(void) address;
	(void) data;
	(void) length;
	return false;
#endif
}

void fillPsramPattern(
    PsramTestBuffer& buffer,
    std::uint32_t address,
    std::size_t length,
    std::uint8_t seed) {
	for (std::size_t index = 0; index < length; ++index) {
#if defined(LEXXOS_PLATFORM_PICO)
		buffer[index] = psramPatternByte(address, index, seed);
#else
		buffer[index] = static_cast<std::uint8_t>((address + static_cast<std::uint32_t>(index)) ^ seed);
#endif
	}
}

bool verifyPsramBuffer(
    PsramTestBuffer const& expected,
    PsramTestBuffer const& actual,
    std::size_t length,
    std::size_t& mismatchOffset) {
	for (std::size_t index = 0; index < length; ++index) {
		if (expected[index] != actual[index]) {
			mismatchOffset = index;
			return false;
		}
	}
	return true;
}

bool idLooksIdle(std::array<std::uint8_t, 8> const& id, std::uint8_t value) {
	for (auto const byte : id) {
		if (byte != value) {
			return false;
		}
	}
	return true;
}

} // namespace lexxos::apps::picocalc_programs::detail
