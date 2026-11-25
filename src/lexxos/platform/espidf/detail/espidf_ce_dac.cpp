#include <lexxos/platform/espidf/detail/espidf_ce_dac.h>

#include <new>

#include <driver/dac_oneshot.h>

namespace lexxos::platform::espidf {

namespace {

	class EspAnalogOutput final : public devices::AnalogOutput {
	public:
		bool initialize(AnalogOutputConfig const& config) {
			// Порт классического ESP32: DAC1 = GPIO25, DAC2 = GPIO26.
			if ((config.pin.number != 25 && config.pin.number != 26) || config.resolutionBits != 8 || config.initialCode > maxCode()) {
				return false;
			}

			dac_oneshot_config_t setup{};
			setup.chan_id = config.pin.number == 25 ? DAC_CHAN_0 : DAC_CHAN_1;
			// ESP-IDF сам резервирует канал и отклоняет повторное владение.
			if (dac_oneshot_new_channel(&setup, &_handle) != ESP_OK) {
				return false;
			}

			return writeRaw(config.initialCode);
		}

		~EspAnalogOutput() override {
			if (_handle) {
				// Отключаем канал; удержание уровня после удаления не гарантируется.
				dac_oneshot_del_channel(_handle);
			}
		}

		bool isReady() const override { return _handle != nullptr; }

		std::uint32_t maxCode() const override { return 255; }

		bool writeRaw(std::uint32_t code) override {
			if (!_handle || code > maxCode()) {
				return false;
			}

			_known = dac_oneshot_output_voltage(_handle, static_cast<std::uint8_t>(code)) == ESP_OK;
			if (_known) {
				_code = code;
			}

			return _known;
		}

		bool currentCode(std::uint32_t& code) const override {
			if (!_known) {
				return false;
			}
			code = _code;
			return true;
		}

	private:
		dac_oneshot_handle_t _handle{};
		std::uint32_t _code{};
		bool _known{};
	};

} // namespace

std::unique_ptr<devices::AnalogOutput> createCeAnalogOutput(AnalogOutputConfig const& config) {
	std::unique_ptr<EspAnalogOutput> output{new (std::nothrow) EspAnalogOutput};
	if (!output || !output->initialize(config)) {
		return nullptr;
	}
	return output;
}

} // namespace lexxos::platform::espidf
