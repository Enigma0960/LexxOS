#include <lexxos/devices/espidf/espidf_spi_bus.h>

#include <algorithm>
#include <array>
#include <cstring>

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lexxos::devices {
namespace {

	constexpr std::size_t maxTransferBytes = 4096;
	constexpr std::size_t readFillChunkBytes = 64;

	std::array<lexxos::devices::espidf::detail::SpiHostState, 4> g_hosts{};

	spi_host_device_t toHost(int index) {
		return index == 1 ? SPI3_HOST : SPI2_HOST;
	}

	std::size_t hostIndex(spi_host_device_t host) {
		auto const value = static_cast<int>(host);
		if (value < 0 || value >= static_cast<int>(g_hosts.size())) {
			return 0;
		}
		return static_cast<std::size_t>(value);
	}

	gpio_num_t toGpio(lexxos::platform::GpioPin pin) {
		return static_cast<gpio_num_t>(pin.number);
	}

	int toIoNumber(lexxos::platform::GpioPin pin) {
		return pin.isValid() ? pin.number : -1;
	}

	bool sameBusPins(lexxos::platform::SpiPins const& left, lexxos::platform::SpiPins const& right) {
		return left.index == right.index && left.sck.number == right.sck.number && left.mosi.number == right.mosi.number && left.miso.number == right.miso.number;
	}

	bool isBusValid(lexxos::platform::SpiPins const& pins) {
		return pins.sck.isValid() && pins.mosi.isValid();
	}

	bool configureChipSelect(lexxos::platform::GpioPin pin) {
		if (!pin.isValid()) {
			return true;
		}

		gpio_config_t config{};
		config.pin_bit_mask = 1ULL << static_cast<std::uint32_t>(pin.number);
		config.mode = GPIO_MODE_OUTPUT;
		config.pull_up_en = GPIO_PULLUP_DISABLE;
		config.pull_down_en = GPIO_PULLDOWN_DISABLE;
		config.intr_type = GPIO_INTR_DISABLE;
		if (gpio_config(&config) != ESP_OK) {
			return false;
		}
		return gpio_set_level(toGpio(pin), 1) == ESP_OK;
	}

	bool configureMisoPullup(lexxos::platform::GpioPin pin) {
		if (!pin.isValid()) {
			return true;
		}
		return gpio_pullup_en(toGpio(pin)) == ESP_OK;
	}

	bool initializeHost(spi_host_device_t host, lexxos::platform::SpiPins const& pins) {
		auto& state = g_hosts[hostIndex(host)];
		if (state.initialized) {
			return sameBusPins(state.pins, pins);
		}

		spi_bus_config_t config{};
		config.mosi_io_num = toIoNumber(pins.mosi);
		config.miso_io_num = toIoNumber(pins.miso);
		config.sclk_io_num = toIoNumber(pins.sck);
		config.quadwp_io_num = -1;
		config.quadhd_io_num = -1;
		config.max_transfer_sz = maxTransferBytes;

		auto const result = spi_bus_initialize(host, &config, SPI_DMA_CH_AUTO);
		if (result != ESP_OK) {
			return false;
		}

		state.initialized = true;
		state.pins = pins;
		return true;
	}

} // namespace

EspIdfSpiBus::~EspIdfSpiBus() {
	if (_device != nullptr) {
		(void) spi_bus_remove_device(_device);
		_device = nullptr;
	}
	if (_mutex != nullptr) {
		vSemaphoreDelete(_mutex);
		_mutex = nullptr;
	}
}

bool EspIdfSpiBus::initialize(lexxos::platform::SpiPins const& pins) {
	if (!isBusValid(pins)) {
		_ready = false;
		return false;
	}

	if (_mutex == nullptr) {
		_mutex = xSemaphoreCreateRecursiveMutex();
		if (_mutex == nullptr) {
			_ready = false;
			return false;
		}
	}

	_pins = pins;
	_host = toHost(pins.index);
	_mode = SpiClockMode::Mode0;
	_selectLockDepth = 0;

	if (!configureChipSelect(pins.cs) || !configureMisoPullup(pins.miso) || !initializeHost(_host, pins)) {
		_ready = false;
		return false;
	}

	_ready = _configureDevice();
	return _ready;
}

bool EspIdfSpiBus::isReady() const {
	return _ready && _device != nullptr;
}

bool EspIdfSpiBus::setBaudRate(std::uint32_t baudRate) {
	if (baudRate == 0) {
		return false;
	}

	if (!isReady()) {
		_pins.baudRate = baudRate;
		return true;
	}

	if (_pins.baudRate == baudRate) {
		return true;
	}

	if (!_lock()) {
		return false;
	}

	_pins.baudRate = baudRate;
	auto const result = _configureDevice();
	_unlock();
	return result;
}

bool EspIdfSpiBus::setClockMode(SpiClockMode mode) {
	if (!isReady()) {
		_mode = mode;
		return true;
	}

	if (_mode == mode) {
		return true;
	}

	if (!_lock()) {
		return false;
	}

	_mode = mode;
	auto const result = _configureDevice();
	_unlock();
	return result;
}

SpiClockMode EspIdfSpiBus::clockMode() const {
	return _mode;
}

void EspIdfSpiBus::select() {
	if (!isReady() || !_lock()) {
		return;
	}
	++_selectLockDepth;
	if (_pins.cs.isValid()) {
		(void) gpio_set_level(toGpio(_pins.cs), 0);
	}
}

void EspIdfSpiBus::deselect() {
	if (!isReady() || _selectLockDepth == 0) {
		return;
	}
	if (_pins.cs.isValid()) {
		(void) gpio_set_level(toGpio(_pins.cs), 1);
	}
	--_selectLockDepth;
	_unlock();
}

bool EspIdfSpiBus::transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (size == 0) {
		return true;
	}
	if (tx == nullptr || rx == nullptr) {
		return false;
	}
	return _transmit(tx, rx, size);
}

bool EspIdfSpiBus::write(std::uint8_t const* data, std::size_t size) {
	if (size == 0) {
		return true;
	}
	if (data == nullptr) {
		return false;
	}
	return _transmit(data, nullptr, size);
}

bool EspIdfSpiBus::read(std::uint8_t fill, std::uint8_t* data, std::size_t size) {
	if (size == 0) {
		return true;
	}
	if (data == nullptr || !isReady() || !_lock()) {
		return false;
	}

	std::array<std::uint8_t, readFillChunkBytes> filler{};
	filler.fill(fill);
	std::size_t offset = 0;
	bool result = true;
	while (offset < size) {
		auto const chunk = std::min<std::size_t>(filler.size(), size - offset);
		if (!_transmitLocked(filler.data(), data + offset, chunk)) {
			result = false;
			break;
		}
		offset += chunk;
	}

	_unlock();
	return result;
}

bool EspIdfSpiBus::beginTransaction() {
	return isReady() && _lock();
}

void EspIdfSpiBus::endTransaction() {
	_unlock();
}

bool EspIdfSpiBus::_lock() {
	return _mutex != nullptr && xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
}

void EspIdfSpiBus::_unlock() {
	if (_mutex != nullptr) {
		(void) xSemaphoreGiveRecursive(_mutex);
	}
}

bool EspIdfSpiBus::_configureDevice() {
	if (_device != nullptr) {
		(void) spi_bus_remove_device(_device);
		_device = nullptr;
	}

	spi_device_interface_config_t config{};
	config.clock_speed_hz = static_cast<int>(_pins.baudRate);
	config.mode = static_cast<std::uint8_t>(_mode);
	config.spics_io_num = -1;
	config.queue_size = 1;

	return spi_bus_add_device(_host, &config, &_device) == ESP_OK;
}

bool EspIdfSpiBus::_transmit(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (!isReady() || !_lock()) {
		return false;
	}

	std::size_t offset = 0;
	bool result = true;
	while (offset < size) {
		auto const chunk = std::min<std::size_t>(maxTransferBytes, size - offset);
		if (!_transmitLocked(tx != nullptr ? tx + offset : nullptr, rx != nullptr ? rx + offset : nullptr, chunk)) {
			result = false;
			break;
		}
		offset += chunk;
	}

	_unlock();
	return result;
}

bool EspIdfSpiBus::_transmitLocked(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (_device == nullptr || size == 0) {
		return size == 0;
	}

	spi_transaction_t transaction{};
	transaction.length = size * 8U;
	transaction.rxlength = rx != nullptr ? size * 8U : 0U;
	transaction.tx_buffer = tx;
	transaction.rx_buffer = rx;

	return spi_device_polling_transmit(_device, &transaction) == ESP_OK;
}

} // namespace lexxos::devices
