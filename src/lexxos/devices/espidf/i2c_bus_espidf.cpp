#include <lexxos/devices/bus/i2c_bus.h>

#include <cstdint>

#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <lexxos/platform/board.h>

namespace lexxos::devices {

namespace {

	constexpr TickType_t i2cTimeoutTicks = pdMS_TO_TICKS(100);

	i2c_port_t toPort(int index) {
		return index == 1 ? I2C_NUM_1 : I2C_NUM_0;
	}

	gpio_num_t toGpio(lexxos::platform::GpioPin pin) {
		return static_cast<gpio_num_t>(pin.number);
	}

	class EspIdfI2cBus final : public I2cBus {
	public:
		~EspIdfI2cBus() override {
			if (_ready) {
				(void) i2c_driver_delete(_port);
			}
			if (_mutex != nullptr) {
				vSemaphoreDelete(_mutex);
			}
		}

		bool initialize(lexxos::platform::I2cPins const& pins) override {
			if (!pins.isValid()) {
				return false;
			}

			if (_mutex == nullptr) {
				_mutex = xSemaphoreCreateRecursiveMutex();
				if (_mutex == nullptr) {
					return false;
				}
			}

			if (_ready) {
				(void) i2c_driver_delete(_port);
				_ready = false;
			}

			_port = toPort(pins.index);
			i2c_config_t config{};
			config.mode = I2C_MODE_MASTER;
			config.sda_io_num = toGpio(pins.sda);
			config.scl_io_num = toGpio(pins.scl);
			config.sda_pullup_en = GPIO_PULLUP_ENABLE;
			config.scl_pullup_en = GPIO_PULLUP_ENABLE;
			config.master.clk_speed = pins.baudRate;

			if (i2c_param_config(_port, &config) != ESP_OK) {
				return false;
			}
			if (i2c_driver_install(_port, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) {
				return false;
			}

			_ready = true;
			return true;
		}

		[[nodiscard]] bool isReady() const override {
			return _ready;
		}

		bool probe(std::uint8_t address) override {
			if (!_ready || !_lock()) {
				return false;
			}

			auto const command = i2c_cmd_link_create();
			if (command == nullptr) {
				_unlock();
				return false;
			}

			(void) i2c_master_start(command);
			(void) i2c_master_write_byte(command, static_cast<std::uint8_t>((address << 1U) | I2C_MASTER_WRITE), true);
			(void) i2c_master_stop(command);
			auto const result = i2c_master_cmd_begin(_port, command, i2cTimeoutTicks);
			i2c_cmd_link_delete(command);
			_unlock();
			return result == ESP_OK;
		}

		bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override {
			if (!_ready || data == nullptr || size == 0 || !_lock()) {
				return false;
			}
			auto const result = i2c_master_write_to_device(_port, address, data, size, i2cTimeoutTicks);
			_unlock();
			return result == ESP_OK;
		}

		bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override {
			if (!_ready || data == nullptr || size == 0 || !_lock()) {
				return false;
			}
			auto const result = i2c_master_read_from_device(_port, address, data, size, i2cTimeoutTicks);
			_unlock();
			return result == ESP_OK;
		}

		bool beginTransaction() override {
			return _ready && _lock();
		}

		void endTransaction() override {
			_unlock();
		}

	private:
		bool _lock() {
			return _mutex != nullptr && xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
		}

		void _unlock() {
			if (_mutex != nullptr) {
				(void) xSemaphoreGiveRecursive(_mutex);
			}
		}

		i2c_port_t _port{I2C_NUM_0};
		SemaphoreHandle_t _mutex{nullptr};
		bool _ready{false};
	};

} // namespace

I2cBus& debugI2cBus() {
	static EspIdfI2cBus bus;
	return bus;
}

I2cBus& systemI2cBus() {
	static EspIdfI2cBus bus;
	return bus;
}

} // namespace lexxos::devices
