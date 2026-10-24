#include <lexxos/devices/pico/pico_i2c_bus.h>

#include <atomic>

#include <hardware/gpio.h>
#include <pico/time.h>

#include <lexxos/board_config.h>
#include <lexxos/platform/platform.h>

namespace {

constexpr std::uint32_t i2cTransactionTimeoutUs = 20000;
constexpr std::uint8_t i2cRecoveryClockPulses = 9;
constexpr std::uint32_t i2cRecoveryPulseDelayUs = 5;
constexpr std::uint32_t picoCalcPassiveRecoveryDelayUs = 1000;

i2c_inst_t* toPicoI2c(int index) {
	return index == 1 ? i2c1 : i2c0;
}

uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
	return static_cast<uint>(pin.number);
}

#if BOARD_FAMILY_ENABLED(PICOCALC)
void writeText(std::string_view text) {
	lexxos::platform::writeConsole(text);
}

void writeUnsigned(std::uint32_t value) {
	char buffer[10]{};
	std::size_t position = sizeof(buffer);
	do {
		buffer[--position] = static_cast<char>('0' + (value % 10U));
		value /= 10U;
	} while (value != 0 && position != 0);
	writeText({buffer + position, sizeof(buffer) - position});
}

void writePicoCalcI2cInitDiagnostic(lexxos::platform::I2cPins const& pins, bool ready, unsigned int actualBaudRate) {
	writeText("picocalc i2c: init index=");
	writeUnsigned(static_cast<std::uint32_t>(pins.index));
	writeText(" sda=");
	writeUnsigned(static_cast<std::uint32_t>(pins.sda.number));
	writeText(" scl=");
	writeUnsigned(static_cast<std::uint32_t>(pins.scl.number));
	writeText(" requested=");
	writeUnsigned(pins.baudRate);
	writeText(" actual=");
	writeUnsigned(actualBaudRate);
	writeText(" ready=");
	writeText(ready ? "yes" : "no");
	writeText("\r\n");
}
#endif

} // namespace

namespace lexxos::devices {

bool PicoI2cBus::initialize(lexxos::platform::I2cPins const& pins) {
	bool const pinsValid = pins.isValid();
	bool const mutexReady = _mutex.initialize();
	if (!pinsValid || !mutexReady) {
		_ready = false;
#if BOARD_FAMILY_ENABLED(PICOCALC)
		writePicoCalcI2cInitDiagnostic(pins, false, 0);
#endif
		return false;
	}

	_pins = pins;
	_instance = toPicoI2c(pins.index);
	unsigned int const actualBaudRate = i2c_init(_instance, pins.baudRate);
	_configureI2cPins();
	_ready = true;
#if BOARD_FAMILY_ENABLED(PICOCALC)
	writePicoCalcI2cInitDiagnostic(_pins, true, actualBaudRate);
#endif
	return true;
}

bool PicoI2cBus::isReady() const {
	return _ready && _instance != nullptr;
}

bool PicoI2cBus::probe(std::uint8_t address) {
	if (!isReady()) {
		return false;
	}

	I2cBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	std::uint8_t value = 0;
	return i2c_read_timeout_us(_instance, address, &value, 1, false, i2cTransactionTimeoutUs) >= 0;
}

bool PicoI2cBus::write(std::uint8_t address, std::uint8_t const* data, std::size_t size) {
	if (!isReady() || data == nullptr || size == 0) {
		return false;
	}

	I2cBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	auto const written = i2c_write_timeout_us(_instance,
	    address,
	    data,
	    static_cast<std::size_t>(size),
	    false,
	    i2cTransactionTimeoutUs);
	if (written == static_cast<int>(size)) {
		return true;
	}

	_recoverBus();
	return false;
}

bool PicoI2cBus::read(std::uint8_t address, std::uint8_t* data, std::size_t size) {
	if (!isReady() || data == nullptr || size == 0) {
		return false;
	}

	I2cBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	auto const read = i2c_read_timeout_us(_instance,
	    address,
	    data,
	    static_cast<std::size_t>(size),
	    false,
	    i2cTransactionTimeoutUs);
	if (read == static_cast<int>(size)) {
		return true;
	}

	_recoverBus();
	return false;
}

bool PicoI2cBus::beginTransaction() {
	return isReady() && _mutex.lock();
}

void PicoI2cBus::endTransaction() {
	_mutex.unlock();
}

void PicoI2cBus::_configureI2cPins() {
	if (!_pins.isValid()) {
		return;
	}

	gpio_set_function(toPicoGpioPin(_pins.sda), GPIO_FUNC_I2C);
	gpio_set_function(toPicoGpioPin(_pins.scl), GPIO_FUNC_I2C);
	gpio_pull_up(toPicoGpioPin(_pins.sda));
	gpio_pull_up(toPicoGpioPin(_pins.scl));
}

void PicoI2cBus::_recoverBus() {
	if (_instance == nullptr || !_pins.isValid()) {
		_ready = false;
		return;
	}

	auto const sda = toPicoGpioPin(_pins.sda);
	auto const scl = toPicoGpioPin(_pins.scl);

	i2c_deinit(_instance);

#if BOARD_FAMILY_ENABLED(PICOCALC)
	// PicoCalc southbridge owns the same bus as the power button controller.
	// Avoid invasive manual SCL clocking here: if the southbridge is busy or
	// stretching/resetting, driving SCL as a GPIO output can make the power MCU
	// appear stuck. Leave both lines released with pull-ups, then re-enable I2C.
	gpio_set_function(sda, GPIO_FUNC_SIO);
	gpio_set_function(scl, GPIO_FUNC_SIO);
	gpio_set_dir(sda, GPIO_IN);
	gpio_set_dir(scl, GPIO_IN);
	gpio_pull_up(sda);
	gpio_pull_up(scl);
	sleep_us(picoCalcPassiveRecoveryDelayUs);
#else
	gpio_set_function(sda, GPIO_FUNC_SIO);
	gpio_set_function(scl, GPIO_FUNC_SIO);
	gpio_pull_up(sda);
	gpio_pull_up(scl);

	gpio_set_dir(sda, GPIO_IN);
	gpio_set_dir(scl, GPIO_OUT);
	gpio_put(scl, 1);
	sleep_us(i2cRecoveryPulseDelayUs);

	for (std::uint8_t index = 0; index < i2cRecoveryClockPulses; ++index) {
		gpio_put(scl, 0);
		sleep_us(i2cRecoveryPulseDelayUs);
		gpio_put(scl, 1);
		sleep_us(i2cRecoveryPulseDelayUs);
		if (gpio_get(sda)) {
			break;
		}
	}

	gpio_set_dir(sda, GPIO_OUT);
	gpio_put(sda, 0);
	sleep_us(i2cRecoveryPulseDelayUs);
	gpio_put(scl, 1);
	sleep_us(i2cRecoveryPulseDelayUs);
	gpio_put(sda, 1);
	sleep_us(i2cRecoveryPulseDelayUs);
#endif

	i2c_init(_instance, _pins.baudRate);
	_configureI2cPins();
	_ready = true;
}

} // namespace lexxos::devices
