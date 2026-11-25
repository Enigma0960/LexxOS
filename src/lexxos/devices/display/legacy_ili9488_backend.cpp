#include <lexxos/devices/display/legacy_ili9488_backend.h>

#include <lexxos/feature.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_RK3506)
	#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>
#endif

#if defined(LEXXOS_PLATFORM_PICO)
	#include <hardware/dma.h>
	#include <hardware/gpio.h>
	#include <hardware/spi.h>
	#include <pico/platform.h>
#endif

namespace {

constexpr std::size_t dmaWriteThresholdBytes = 96;

#if defined(LEXXOS_PLATFORM_PICO)
spi_inst_t* displaySpiInstance(int index) {
	return index == 1 ? spi1 : spi0;
}

void waitForDisplaySpiIdle(spi_inst_t* spi) {
	if (spi == nullptr) {
		return;
	}

	while (spi_is_busy(spi)) {
		tight_loop_contents();
	}
}

bool writeDisplaySpiDma(spi_inst_t* spi, int channel, std::uint8_t const* data, std::size_t size) {
	if (spi == nullptr || channel < 0 || data == nullptr || size == 0) {
		return false;
	}

	auto config = dma_channel_get_default_config(channel);
	channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
	channel_config_set_read_increment(&config, true);
	channel_config_set_write_increment(&config, false);
	channel_config_set_dreq(&config, spi_get_dreq(spi, true));
	dma_channel_configure(channel, &config, &spi_get_hw(spi)->dr, data, static_cast<std::uint32_t>(size), true);
	dma_channel_wait_for_finish_blocking(channel);
	waitForDisplaySpiIdle(spi);

	return true;
}

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}
#endif

} // namespace

namespace lexxos::devices {

LegacyIli9488Backend::LegacyIli9488Backend(platform::SpiTftDisplayPins config, platform::PlatformServices const& services)
    : _config(config)
    , _platformServices(services) {
}

LegacyIli9488Backend::~LegacyIli9488Backend() {
#if defined(LEXXOS_PLATFORM_PICO)
	if (_dmaChannel >= 0) {
		dma_channel_unclaim(_dmaChannel);
	}
#endif
}

bool LegacyIli9488Backend::initialize() {
	if (!_config.isValid()) {
		return false;
	}

	_hardwareSpi = false;
#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] GPIO CS/DC/RESET begin\n");
#endif

	if (!configureOutput(_config.cs, true) || !configureOutput(_config.dc, false) || !configureOutput(_config.reset, true)) {
		return false;
	}

#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[ili9488] GPIO done; SPI0 begin\n");
#endif

	if (_config.spiIndex >= 0) {
#if defined(LEXXOS_PLATFORM_PICO)
		auto* spi = displaySpiInstance(_config.spiIndex);
		spi_init(spi, _config.spiBaudRate);
		spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
		gpio_set_function(toPicoGpioPin(_config.sck), GPIO_FUNC_SPI);
		gpio_set_function(toPicoGpioPin(_config.mosi), GPIO_FUNC_SPI);
		gpio_set_drive_strength(toPicoGpioPin(_config.sck), GPIO_DRIVE_STRENGTH_8MA);
		gpio_set_drive_strength(toPicoGpioPin(_config.mosi), GPIO_DRIVE_STRENGTH_8MA);
		_hardwareSpi = true;

		if (_dmaChannel < 0) {
			_dmaChannel = dma_claim_unused_channel(false);
		}

		return true;
#elif defined(LEXXOS_PLATFORM_RK3506)
		if (_config.spiIndex != 0 || _config.sck.number != 7 || _config.mosi.number != 6 || !lexxos::platform::rk3506::lcdSpi0Initialize(_config.spiBaudRate)) {
			return false;
		}

		_hardwareSpi = true;
		return true;
#else
		return false;
#endif
	}

	return configureOutput(_config.sck, false) && configureOutput(_config.mosi, false);
}

bool LegacyIli9488Backend::write(std::uint8_t const* data, std::size_t size) {
	if (!size) {
		return true;
	}

	if (!data) {
		return false;
	}
#if defined(LEXXOS_PLATFORM_PICO)
	if (_hardwareSpi) {
		// spi_write_blocking(displaySpiInstance(_config.spiIndex), data, size);

		auto* spi = displaySpiInstance(_config.spiIndex);
		if (_dmaChannel >= 0 && size >= dmaWriteThresholdBytes && writeDisplaySpiDma(spi, _dmaChannel, data, size)) {
			return true;
		}

		spi_write_blocking(spi, data, size);
		waitForDisplaySpiIdle(spi);
		return true;
	}
#elif defined(LEXXOS_PLATFORM_RK3506)
	if (_hardwareSpi) {
		return lexxos::platform::rk3506::lcdSpi0Write(data, size);
	}
#endif
	for (std::size_t index = 0; index < size; ++index) {
		auto const value = data[index];
		for (int bit = 7; bit >= 0; --bit) {
			writeGpio(_config.sck, false);
			writeGpio(_config.mosi, ((value >> bit) & 0x01U) != 0);
			writeGpio(_config.sck, true);
		}

		writeGpio(_config.sck, false);
	}

	return true;
}

bool LegacyIli9488Backend::supportsRgb565Words() const {
#if defined(LEXXOS_PLATFORM_RK3506)
	return _hardwareSpi && platform::rk3506::lcdSpi0Rgb565Direct16Enabled();
#else
	return false;
#endif
}

bool LegacyIli9488Backend::writeRgb565Words(std::uint16_t const* data, std::size_t pixels) {
#if defined(LEXXOS_PLATFORM_RK3506)
	return _hardwareSpi && platform::rk3506::lcdSpi0WriteRgb565Words(data, pixels);
#else
	(void) data;
	(void) pixels;
	return false;
#endif
}

bool LegacyIli9488Backend::configureBusSpeed(std::uint32_t hz) {
	if (!_hardwareSpi || !hz) {
		return false;
	}

#if defined(LEXXOS_PLATFORM_PICO)
	auto* spi = displaySpiInstance(_config.spiIndex);
	auto const actual = spi_set_baudrate(spi, hz);
	if (actual == 0) {
		return false;
	}

	_config.spiBaudRate = actual;
	return true;
#elif defined(LEXXOS_PLATFORM_RK3506)
	if (!lexxos::platform::rk3506::lcdSpi0SetClockHz(hz)) {
		return false;
	}

	_config.spiBaudRate = lexxos::platform::rk3506::lcdSpi0ClockHz();
	return true;
#else
	(void) hz;
	return false;
#endif
}

bool LegacyIli9488Backend::configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const {
	auto const& gpio = _platformServices.gpio;
	return gpio.configureOutput != nullptr && gpio.configureOutput(pin, initialValue);
}

void LegacyIli9488Backend::writeGpio(lexxos::platform::GpioPin pin, bool value) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(pin, value);
	}
}

void LegacyIli9488Backend::sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = _platformServices.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

std::uint32_t LegacyIli9488Backend::busSpeedHz() const {
	return _hardwareSpi ? _config.spiBaudRate : 0;
}

void LegacyIli9488Backend::select(bool selected) {
	writeGpio(_config.cs, !selected);
}

void LegacyIli9488Backend::dataMode(bool data) {
	writeGpio(_config.dc, data);
}

void LegacyIli9488Backend::resetLine(bool high) {
	writeGpio(_config.reset, high);
}

} // namespace lexxos::devices
