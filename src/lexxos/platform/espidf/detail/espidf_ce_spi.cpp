#include <lexxos/platform/espidf/detail/espidf_ce_spi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <new>

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace lexxos::platform::espidf {

namespace {

	constexpr std::size_t chunkSize = 64;
	constexpr int lockTimeoutMs = 100;

	bool usablePin(int pin, bool output) {
		if (pin < 0 || pin >= GPIO_NUM_MAX || (pin >= 6 && pin <= 11)) {
			return false;
		}
		return output ? GPIO_IS_VALID_OUTPUT_GPIO(pin) : GPIO_IS_VALID_GPIO(pin);
	}

	bool validBaud(std::uint32_t baud) {
		return baud >= 1000 && baud <= 40000000;
	}

	class CeSpiController final : public devices::SpiController {
	public:
		~CeSpiController() override {
			if (_ready) {
				spi_bus_free(_host);
			}

			if (_mutex) {
				vSemaphoreDelete(_mutex);
			}
		}

		bool initialize(SpiInterfacePins const& pins) override {
			if (_ready || !_mutex || pins.index < 0 || pins.index > 1 || !usablePin(pins.sck.number, true) || (pins.mosi.number != -1 && !usablePin(pins.mosi.number, true)) || (pins.miso.number != -1 && !usablePin(pins.miso.number, false)) || (pins.mosi.number == -1 && pins.miso.number == -1) || pins.sck.number == pins.mosi.number || pins.sck.number == pins.miso.number || (pins.mosi.number >= 0 && pins.mosi.number == pins.miso.number)) {
				return false;
			}

			_host = pins.index == 0 ? SPI2_HOST : SPI3_HOST;
			spi_bus_config_t config{};
			config.sclk_io_num = pins.sck.number;
			config.mosi_io_num = pins.mosi.number;
			config.miso_io_num = pins.miso.number;
			config.quadwp_io_num = -1;
			config.quadhd_io_num = -1;
			config.max_transfer_sz = chunkSize;
			if (spi_bus_initialize(_host, &config, SPI_DMA_DISABLED) != ESP_OK) {
				return false;
			}

			_pins = pins;
			_ready = true;
			return true;
		}

		std::unique_ptr<devices::SpiBus> createDevice(SpiDeviceConfig const& config) override;

		bool acquire(devices::SpiBus const* device) {
			if (!_ready || xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(lockTimeoutMs)) != pdTRUE) {
				return false;
			}

			// Вложенное обращение к другому CS из той же задачи также запрещено.
			if (_owner && _owner != device) {
				xSemaphoreGiveRecursive(_mutex);
				return false;
			}

			_owner = device;
			++_depth;
			return true;
		}

		void release(devices::SpiBus const* device) {
			if (xSemaphoreGetMutexHolder(_mutex) != xTaskGetCurrentTaskHandle() || _owner != device) {
				return;
			}

			if (--_depth == 0) {
				_owner = nullptr;
			}

			xSemaphoreGiveRecursive(_mutex);
		}

		devices::BusTransactionQueue queue;

		spi_host_device_t host() const { return _host; }

		SpiInterfacePins const& pins() const { return _pins; }

	private:
		SemaphoreHandle_t _mutex{xSemaphoreCreateRecursiveMutex()};
		spi_host_device_t _host{SPI2_HOST};
		SpiInterfacePins _pins{};
		bool _ready{};
		devices::SpiBus const* _owner{};
		unsigned _depth{};
	};

	class CeSpiDevice final : public devices::SpiBus {
	public:
		CeSpiDevice(CeSpiController& controller, devices::SpiClockMode mode)
		    : SpiBus(controller.queue)
		    , _controller(controller)
		    , _mode(mode) {}

		~CeSpiDevice() override {
			if (_device) {
				spi_bus_remove_device(_device);
			}

			if (_cs >= 0) {
				gpio_set_level(static_cast<gpio_num_t>(_cs), 1);
				gpio_reset_pin(static_cast<gpio_num_t>(_cs));
			}
		}

		bool initialize(SpiPins const& pins) override {
			auto const& lines = _controller.pins();
			if (_ready || pins.index != lines.index || pins.sck.number != lines.sck.number || pins.mosi.number != lines.mosi.number || pins.miso.number != lines.miso.number || !usablePin(pins.cs.number, true) || pins.cs.number == lines.sck.number || pins.cs.number == lines.mosi.number || pins.cs.number == lines.miso.number || !validBaud(pins.baudRate)) {
				return false;
			}

			auto cs = static_cast<gpio_num_t>(pins.cs.number);
			// Сначала неактивный уровень в регистре, затем включение выхода.
			if (gpio_set_level(cs, 1) != ESP_OK) {
				return false;
			}

			_cs = pins.cs.number;
			gpio_config_t gpio{};
			gpio.pin_bit_mask = 1ULL << pins.cs.number;
			gpio.mode = GPIO_MODE_OUTPUT;
			gpio.intr_type = GPIO_INTR_DISABLE;
			if (gpio_config(&gpio) != ESP_OK) {
				return false;
			}

			if (!configure(_mode.load(), pins.baudRate)) {
				return false;
			}

			_ready = true;
			return true;
		}

		bool isReady() const override { return _ready; }

		bool beginTransaction() override { return _ready && _controller.acquire(this); }

		void endTransaction() override { _controller.release(this); }

		bool setBaudRate(std::uint32_t baud) override {
			if (!validBaud(baud) || !beginTransaction()) {
				return false;
			}

			bool ok = baud == _baud || (_selected == 0 && configure(_mode.load(), baud));
			endTransaction();
			return ok;
		}

		bool setClockMode(devices::SpiClockMode mode) override {
			if (static_cast<unsigned>(mode) > 3 || !beginTransaction()) {
				return false;
			}

			bool ok = mode == _mode.load() || (_selected == 0 && configure(mode, _baud));
			endTransaction();
			return ok;
		}

		devices::SpiClockMode clockMode() const override { return _mode.load(); }

		void select() override {
			if (!beginTransaction()) {
				return;
			}

			if (gpio_set_level(static_cast<gpio_num_t>(_cs), 0) != ESP_OK) {
				endTransaction();
				return;
			}

			++_selected; // Блокировка удерживается до соответствующего deselect().
		}

		void deselect() override {
			if (!beginTransaction()) {
				return;
			}

			if (_selected) {
				if (--_selected == 0) {
					gpio_set_level(static_cast<gpio_num_t>(_cs), 1);
				}

				endTransaction(); // Блокировка, удержанная select().
			}

			endTransaction();
		}

		bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override {
			if (!size) {
				return true;
			}

			if (!tx || !rx || !_controller.pins().mosi.isValid() || !_controller.pins().miso.isValid()) {
				return false;
			}

			return exchange(tx, rx, size, 0);
		}

		bool write(std::uint8_t const* data, std::size_t size) override {
			if (!size) {
				return true;
			}
			return data && _controller.pins().mosi.isValid() && exchange(data, nullptr, size, 0);
		}

		bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) override {
			if (!size) {
				return true;
			}
			return data && _controller.pins().miso.isValid() && exchange(nullptr, data, size, fill);
		}

	private:
		bool configure(devices::SpiClockMode mode, std::uint32_t baud) {
			spi_device_interface_config_t config{};
			config.clock_speed_hz = static_cast<int>(baud);
			config.mode = static_cast<std::uint8_t>(mode);
			config.spics_io_num = -1; // CS принадлежит интервалу select()/deselect().
			config.queue_size = 1;
			spi_device_handle_t replacement{};
			if (spi_bus_add_device(_controller.host(), &config, &replacement) != ESP_OK) {
				return false;
			}

			// Предыдущая конфигурация остаётся рабочей при отказе выделения нового handle.
			if (_device && spi_bus_remove_device(_device) != ESP_OK) {
				spi_bus_remove_device(replacement);
				return false;
			}

			_device = replacement;
			_mode.store(mode);
			_baud = baud;
			return true;
		}

		bool exchange(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size, std::uint8_t fill) {
			if (!beginTransaction()) {
				return false;
			}

			bool ok = _selected != 0;
			std::array<std::uint8_t, chunkSize> filler{};
			filler.fill(fill);
			std::size_t offset = 0;
			while (ok && offset < size) {
				auto const length = std::min(chunkSize, size - offset);
				spi_transaction_t transaction{};
				transaction.length = length * 8;
				transaction.rxlength = rx ? length * 8 : 0;
				transaction.tx_buffer = tx ? tx + offset : filler.data();
				if (!_controller.pins().mosi.isValid()) {
					transaction.tx_buffer = nullptr;
				}

				transaction.rx_buffer = rx ? rx + offset : nullptr;
				ok = spi_device_polling_transmit(_device, &transaction) == ESP_OK;
				offset += length;
			}

			endTransaction();
			return ok;
		}

		CeSpiController& _controller;
		spi_device_handle_t _device{};
		std::atomic<devices::SpiClockMode> _mode{devices::SpiClockMode::Mode0};
		std::uint32_t _baud{};
		int _cs{-1};
		bool _ready{};
		unsigned _selected{};
	};

	std::unique_ptr<devices::SpiBus> CeSpiController::createDevice(SpiDeviceConfig const& config) {
		if (!_ready || static_cast<unsigned>(config.mode) > 3) {
			return {};
		}

		std::unique_ptr<CeSpiDevice> device{new (std::nothrow) CeSpiDevice(*this, config.mode)};
		SpiPins pins{.index = _pins.index, .sck = _pins.sck, .mosi = _pins.mosi, .miso = _pins.miso, .cs = config.cs, .baudRate = config.baudRate};
		if (!device || !device->initialize(pins)) {
			return {};
		}

		return device;
	}

} // namespace

std::unique_ptr<devices::SpiController> createCeSpiController() {
	return std::unique_ptr<devices::SpiController>{new (std::nothrow) CeSpiController};
}

} // namespace lexxos::platform::espidf
