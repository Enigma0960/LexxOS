#include <lexxos/platform/espidf/detail/espidf_ce_i2c.h>

#include <new>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <soc/soc_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lexxos::platform::espidf {

namespace {

	constexpr int timeoutMs = 100;

	bool usablePin(int pin) {
		return pin >= 0 && pin < GPIO_NUM_MAX && GPIO_IS_VALID_OUTPUT_GPIO(pin) && (pin < 6 || pin > 11);
	}

	class CeI2cMaster final : public devices::I2cBus {
	public:
		~CeI2cMaster() override {
			if (_device) {
				i2c_master_bus_rm_device(_device);
			}

			if (_bus) {
				i2c_del_master_bus(_bus);
			}

			if (_lock) {
				vSemaphoreDelete(_lock);
			}
		}

		bool initialize(I2cPins const& pins) override {
			if (_bus || !_lock || pins.index < 0 || pins.index >= SOC_I2C_NUM || !usablePin(pins.sda.number) || !usablePin(pins.scl.number) || pins.sda.number == pins.scl.number || !pins.baudRate || pins.baudRate > 400000) {
				return false;
			}

			i2c_master_bus_config_t config{};
			config.i2c_port = pins.index;
			config.sda_io_num = static_cast<gpio_num_t>(pins.sda.number);
			config.scl_io_num = static_cast<gpio_num_t>(pins.scl.number);
			config.clk_source = I2C_CLK_SRC_DEFAULT;
			config.glitch_ignore_cnt = 7;
			if (i2c_new_master_bus(&config, &_bus) != ESP_OK) {
				return false;
			}

			i2c_device_config_t device{};
			device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
			device.device_address = 0x08; // Заменяется под блокировкой перед обменом.
			device.scl_speed_hz = pins.baudRate;
			if (i2c_master_bus_add_device(_bus, &device, &_device) == ESP_OK) {
				return true;
			}

			i2c_del_master_bus(_bus);
			_bus = nullptr;
			return false;
		}

		bool isReady() const override { return _device != nullptr; }

		bool beginTransaction() override {
			return isReady() && xSemaphoreTakeRecursive(_lock, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
		}

		void endTransaction() override { xSemaphoreGiveRecursive(_lock); }

		bool probe(std::uint8_t address) override {
			if (address > 0x7F || !beginTransaction()) {
				return false;
			}

			bool ok = i2c_master_probe(_bus, address, timeoutMs) == ESP_OK;
			endTransaction();
			return ok;
		}

		bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override {
			return data && size && transfer(address, data, size, nullptr, 0);
		}

		bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override {
			return data && size && transfer(address, nullptr, 0, data, size);
		}

		bool writeRead(std::uint8_t address, std::uint8_t const* tx, std::size_t txSize,
		    std::uint8_t* rx, std::size_t rxSize) override {
			return tx && txSize && rx && rxSize && transfer(address, tx, txSize, rx, rxSize);
		}

	private:
		bool transfer(std::uint8_t address, std::uint8_t const* tx, std::size_t txSize, std::uint8_t* rx, std::size_t rxSize) {
			if (address > 0x7F || !beginTransaction()) {
				return false;
			}

			auto result = i2c_master_device_change_address(_device, address, timeoutMs);
			if (result == ESP_OK) {
				if (tx && rx) {
					result = i2c_master_transmit_receive(_device, tx, txSize, rx, rxSize, timeoutMs);
				} else if (tx) {
					result = i2c_master_transmit(_device, tx, txSize, timeoutMs);
				} else {
					result = i2c_master_receive(_device, rx, rxSize, timeoutMs);
				}
			}

			endTransaction();
			return result == ESP_OK;
		}

		SemaphoreHandle_t _lock{xSemaphoreCreateRecursiveMutex()};
		i2c_master_bus_handle_t _bus{};
		i2c_master_dev_handle_t _device{};
	};

} // namespace

std::unique_ptr<devices::I2cBus> createCeI2cBus() {
	return std::unique_ptr<devices::I2cBus>{new (std::nothrow) CeI2cMaster};
}

} // namespace lexxos::platform::espidf
