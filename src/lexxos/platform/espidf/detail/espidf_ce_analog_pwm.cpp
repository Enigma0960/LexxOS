#include <lexxos/platform/espidf/detail/espidf_ce_analog_pwm.h>

#include <array>
#include <new>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lexxos::platform::espidf {

namespace {

	constexpr auto speed = LEDC_LOW_SPEED_MODE;

	struct AdcUnit {
		adc_oneshot_unit_handle_t handle{};
		std::uint32_t channels{};
	};

	struct PwmTimer {
		unsigned references{};
		std::uint32_t frequency{};
		std::uint8_t bits{};
	};

	// Создаётся только при первом обращении к аналоговой периферии.
	struct Registry {
		StaticSemaphore_t mutexStorage{};
		SemaphoreHandle_t mutex{xSemaphoreCreateMutexStatic(&mutexStorage)};
		std::array<AdcUnit, 2> adc{};
		std::array<PwmTimer, LEDC_TIMER_MAX> timers{};
		std::uint32_t pwmChannels{};
	};

	Registry& registry() {
		static Registry value;
		return value;
	}

	class Guard {
	public:
		Guard() { xSemaphoreTake(registry().mutex, portMAX_DELAY); }

		~Guard() { xSemaphoreGive(registry().mutex); }

		Guard(Guard const&) = delete;
		Guard& operator=(Guard const&) = delete;
	};

	class EspAnalogInput final : public devices::AnalogInput {
	public:
		bool initialize(AnalogInputConfig const& config) {
			if (!config.pin.isValid() || config.resolutionBits < 9 || config.resolutionBits > 12) {
				return false;
			}

			adc_atten_t attenuation;
			switch (config.attenuation) {
				case AnalogAttenuation::Db0  : attenuation = ADC_ATTEN_DB_0; break;
				case AnalogAttenuation::Db2_5: attenuation = ADC_ATTEN_DB_2_5; break;
				case AnalogAttenuation::Db6  : attenuation = ADC_ATTEN_DB_6; break;
				case AnalogAttenuation::Db12 : attenuation = ADC_ATTEN_DB_12; break;
				default                      : return false;
			}

			if (adc_oneshot_io_to_channel(config.pin.number, &_unit, &_channel) != ESP_OK) {
				return false;
			}

			Guard guard;
			auto& unit = registry().adc[static_cast<unsigned>(_unit)];
			auto const mask = std::uint32_t{1} << _channel;
			if (unit.channels & mask) {
				return false;
			}

			if (!unit.handle) {
				adc_oneshot_unit_init_cfg_t setup{};
				setup.unit_id = _unit;
				if (adc_oneshot_new_unit(&setup, &unit.handle) != ESP_OK) {
					return false;
				}
			}

			// С этого момента деструктор освобождает канал и последний экземпляр блока.
			unit.channels |= mask;
			_owned = true;
			adc_oneshot_chan_cfg_t channelConfig{};
			channelConfig.atten = attenuation;
			channelConfig.bitwidth = static_cast<adc_bitwidth_t>(config.resolutionBits);
			if (adc_oneshot_config_channel(unit.handle, _channel, &channelConfig) != ESP_OK) {
				return false;
			}

			adc_cali_line_fitting_efuse_val_t efuse{};
			if (adc_cali_scheme_line_fitting_check_efuse(&efuse) != ESP_OK) {
				return false;
			}

			if (efuse != ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF) {
				adc_cali_line_fitting_config_t calibration{};
				calibration.unit_id = _unit;
				calibration.atten = attenuation;
				calibration.bitwidth = channelConfig.bitwidth;
				if (adc_cali_create_scheme_line_fitting(&calibration, &_calibration) != ESP_OK) {
					return false;
				}
			}

			_maxCode = (std::uint32_t{1} << config.resolutionBits) - 1;
			_ready = true;
			return true;
		}

		~EspAnalogInput() override {
			if (!_owned) {
				return;
			}

			Guard guard;
			if (_calibration) {
				adc_cali_delete_scheme_line_fitting(_calibration);
			}

			auto& unit = registry().adc[static_cast<unsigned>(_unit)];
			unit.channels &= ~(std::uint32_t{1} << _channel);
			if (!unit.channels) {
				adc_oneshot_del_unit(unit.handle);
				unit.handle = nullptr;
			}
		}

		bool isReady() const override { return _ready; }

		std::uint32_t maxCode() const override { return _maxCode; }

		bool hasCalibration() const override { return _calibration != nullptr; }

		bool readRaw(std::uint32_t& code) override {
			if (!_ready) {
				return false;
			}

			Guard guard;
			int raw{};
			if (!read(raw)) {
				return false;
			}

			code = static_cast<std::uint32_t>(raw);
			return true;
		}

		bool readMillivolts(std::uint32_t& millivolts) override {
			if (!_ready || !_calibration) {
				return false;
			}

			Guard guard;
			int raw{}, voltage{};
			if (!read(raw) || adc_cali_raw_to_voltage(_calibration, raw, &voltage) != ESP_OK || voltage < 0) {
				return false;
			}

			millivolts = static_cast<std::uint32_t>(voltage);
			return true;
		}

	private:
		bool read(int& raw) {
			return adc_oneshot_read(registry().adc[static_cast<unsigned>(_unit)].handle, _channel, &raw) == ESP_OK && raw >= 0 && static_cast<std::uint32_t>(raw) <= _maxCode;
		}

		adc_unit_t _unit{};
		adc_channel_t _channel{};
		adc_cali_handle_t _calibration{};
		std::uint32_t _maxCode{};
		bool _owned{}, _ready{};
	};

	void releaseTimer(ledc_timer_t timer) {
		ledc_timer_pause(speed, timer);
		ledc_timer_config_t config{};
		config.speed_mode = speed;
		config.timer_num = timer;
		config.deconfigure = true;
		ledc_timer_config(&config);
	}

	class EspPwmOutput final : public devices::PwmOutput {
	public:
		bool initialize(PwmOutputConfig const& config) {
			if (config.pin.number < 0 || config.pin.number >= GPIO_NUM_MAX || !GPIO_IS_VALID_OUTPUT_GPIO(config.pin.number) || (config.pin.number >= 6 && config.pin.number <= 11) || config.channel >= LEDC_CHANNEL_MAX || config.timer >= LEDC_TIMER_MAX || !config.frequencyHz || config.frequencyHz > 40000000 || !config.resolutionBits || config.resolutionBits > 20 || config.initialDutyPermille > 1000 || (config.activeLevel != GpioActiveLevel::High && config.activeLevel != GpioActiveLevel::Low)) {
				return false;
			}

			Guard guard;
			auto& state = registry();
			auto const mask = std::uint32_t{1} << config.channel;
			if (state.pwmChannels & mask) {
				return false;
			}

			auto& timer = state.timers[config.timer];
			if (timer.references && (timer.frequency != config.frequencyHz || timer.bits != config.resolutionBits)) {
				return false;
			}

			_channel = static_cast<ledc_channel_t>(config.channel);
			_timer = static_cast<ledc_timer_t>(config.timer);
			_pin = config.pin.number;
			_activeLow = config.activeLevel == GpioActiveLevel::Low;
			_bits = config.resolutionBits;
			if (!timer.references) {
				ledc_timer_config_t setup{};
				setup.speed_mode = speed;
				setup.duty_resolution = static_cast<ledc_timer_bit_t>(_bits);
				setup.timer_num = _timer;
				setup.freq_hz = config.frequencyHz;
				setup.clk_cfg = LEDC_USE_APB_CLK;
				if (ledc_timer_config(&setup) != ESP_OK) {
					releaseTimer(_timer);
					return false;
				}

				timer.frequency = config.frequencyHz;
				timer.bits = _bits;
			}

			++timer.references;
			state.pwmChannels |= mask;
			_owned = true;
			ledc_channel_config_t setup{};
			setup.gpio_num = _pin;
			setup.speed_mode = speed;
			setup.channel = _channel;
			setup.timer_sel = _timer;
			setup.flags.output_invert = _activeLow;
			if (ledc_channel_config(&setup) != ESP_OK) {
				return false;
			}

			_frequency = ledc_get_freq(speed, _timer);
			_ready = _frequency != 0;
			return _ready && write(config.initialDutyPermille);
		}

		~EspPwmOutput() override {
			if (!_owned) {
				return;
			}

			Guard guard;
			ledc_stop(speed, _channel, 0);
			ledc_channel_config_t setup{};
			setup.speed_mode = speed;
			setup.channel = _channel;
			setup.deconfigure = true;
			ledc_channel_config(&setup);
			// После освобождения LEDC оставляем нагрузку в неактивном состоянии.
			gpio_set_level(static_cast<gpio_num_t>(_pin), _activeLow ? 1 : 0);
			gpio_config_t idle{};
			idle.pin_bit_mask = std::uint64_t{1} << _pin;
			idle.mode = GPIO_MODE_OUTPUT;
			gpio_config(&idle);
			registry().pwmChannels &= ~(std::uint32_t{1} << _channel);
			auto& timer = registry().timers[static_cast<unsigned>(_timer)];
			if (--timer.references == 0) {
				releaseTimer(_timer);
				timer = {};
			}
		}

		bool isReady() const override { return _ready; }

		std::uint32_t frequencyHz() const override { return _frequency; }

		bool setDutyPermille(std::uint16_t duty) override {
			if (!_ready || duty > 1000) {
				return false;
			}
			Guard guard;
			return write(duty);
		}

		bool dutyPermille(std::uint16_t& duty) const override {
			if (!_dutyKnown) {
				return false;
			}
			duty = _duty;
			return true;
		}

	private:
		bool write(std::uint16_t duty) {
			esp_err_t result;
			if (duty == 0 || duty == 1000) {
				result = ledc_stop(speed, _channel, duty == 1000 ? 1 : 0);
			} else {
				auto const counts = std::uint32_t{1} << _bits;
				auto raw = static_cast<std::uint32_t>((static_cast<std::uint64_t>(duty) * counts + 500) / 1000);
				if (raw >= counts) {
					raw = counts - 1;
				}

				result = ledc_set_duty(speed, _channel, raw);
				if (result == ESP_OK) {
					result = ledc_update_duty(speed, _channel);
				}
			}

			_dutyKnown = result == ESP_OK;
			if (_dutyKnown) {
				_duty = duty;
			}

			return _dutyKnown;
		}

		ledc_channel_t _channel{};
		ledc_timer_t _timer{};
		int _pin{};
		std::uint8_t _bits{};
		std::uint32_t _frequency{};
		std::uint16_t _duty{};
		bool _activeLow{}, _owned{}, _ready{}, _dutyKnown{};
	};

} // namespace

std::unique_ptr<devices::AnalogInput> createCeAnalogInput(AnalogInputConfig const& config) {
	std::unique_ptr<EspAnalogInput> input{new (std::nothrow) EspAnalogInput};
	if (!input || !input->initialize(config)) {
		return nullptr;
	}
	return input;
}

std::unique_ptr<devices::PwmOutput> createCePwmOutput(PwmOutputConfig const& config) {
	std::unique_ptr<EspPwmOutput> output{new (std::nothrow) EspPwmOutput};
	if (!output || !output->initialize(config)) {
		return nullptr;
	}
	return output;
}

} // namespace lexxos::platform::espidf
