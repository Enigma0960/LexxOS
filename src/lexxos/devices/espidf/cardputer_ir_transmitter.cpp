#include <lexxos/devices/espidf/cardputer_ir_transmitter.h>

#include <array>

#include <driver/rmt_common.h>
#include <driver/rmt_encoder.h>
#include <driver/rmt_tx.h>
#include <esp_err.h>
#include <esp_rom_sys.h>

#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint32_t samsungHeaderMarkUs = 4500;
	constexpr std::uint32_t samsungHeaderSpaceUs = 4500;
	constexpr std::uint32_t samsungBitMarkUs = 560;
	constexpr std::uint32_t samsungZeroSpaceUs = 560;
	constexpr std::uint32_t samsungOneSpaceUs = 1690;
	constexpr std::uint32_t samsungFrameGapUs = 45000;
	constexpr std::uint32_t samsungRmtResolutionHz = 1000000;
	constexpr std::size_t samsungFrameSymbolCount = 34;

	[[nodiscard]] rmt_symbol_word_t symbol(std::uint32_t markUs, std::uint32_t spaceUs) {
		rmt_symbol_word_t value{};
		value.level0 = 1;
		value.duration0 = static_cast<std::uint16_t>(markUs);
		value.level1 = 0;
		value.duration1 = static_cast<std::uint16_t>(spaceUs);
		return value;
	}

} // namespace

CardputerIrTransmitter::CardputerIrTransmitter(
    lexxos::platform::InfraredTransmitterPins config,
    lexxos::platform::PlatformServices const& platform)
    : _config(config)
    , _platform(&platform) {
}

bool CardputerIrTransmitter::initialize() {
	_ready = false;
	if (!_config.isValid()) {
		return false;
	}
	_ready = _initializeRmt();
	return _ready;
}

std::string_view CardputerIrTransmitter::name() const {
	return _config.name != nullptr ? std::string_view{_config.name} : std::string_view{"cardputer-ir"};
}

bool CardputerIrTransmitter::isReady() const {
	return _ready;
}

std::uint32_t CardputerIrTransmitter::carrierFrequencyHz() const {
	return _config.carrierFrequencyHz;
}

int CardputerIrTransmitter::outputPinNumber() const {
	return _config.output.number;
}

bool CardputerIrTransmitter::transmitSamsung(std::uint32_t code, std::uint8_t repeats) {
	if (!_ready) {
		return false;
	}

	auto const frames = repeats == 0 ? 1U : static_cast<unsigned>(repeats);
	for (unsigned frame = 0; frame < frames; ++frame) {
		if (!_sendSamsungFrame(code)) {
			return false;
		}
		if (frame + 1U < frames) {
			esp_rom_delay_us(samsungFrameGapUs);
		}
	}
	return true;
}

bool CardputerIrTransmitter::_initializeRmt() {
	if (_rmtChannel == nullptr) {
		rmt_tx_channel_config_t channelConfig{};
		channelConfig.gpio_num = static_cast<gpio_num_t>(_config.output.number);
		channelConfig.clk_src = RMT_CLK_SRC_DEFAULT;
		channelConfig.resolution_hz = samsungRmtResolutionHz;
		channelConfig.mem_block_symbols = 64;
		channelConfig.trans_queue_depth = 1;
		channelConfig.intr_priority = 0;
		channelConfig.flags.init_level = 0;
		rmt_channel_handle_t channel = nullptr;
		if (rmt_new_tx_channel(&channelConfig, &channel) != ESP_OK) {
			_rmtChannel = nullptr;
			return false;
		}
		_rmtChannel = channel;

		rmt_carrier_config_t carrierConfig{};
		carrierConfig.frequency_hz = _config.carrierFrequencyHz == 0 ? 38000U : _config.carrierFrequencyHz;
		carrierConfig.duty_cycle = 0.33F;
		if (rmt_apply_carrier(channel, &carrierConfig) != ESP_OK) {
			return false;
		}
		if (rmt_enable(channel) != ESP_OK) {
			return false;
		}
	}

	if (_rmtEncoder == nullptr) {
		rmt_copy_encoder_config_t encoderConfig{};
		rmt_encoder_handle_t encoder = nullptr;
		if (rmt_new_copy_encoder(&encoderConfig, &encoder) != ESP_OK) {
			_rmtEncoder = nullptr;
			return false;
		}
		_rmtEncoder = encoder;
	}
	return true;
}

bool CardputerIrTransmitter::_sendSamsungFrame(std::uint32_t code) const {
	if (_rmtChannel == nullptr || _rmtEncoder == nullptr) {
		return false;
	}
	auto const channel = static_cast<rmt_channel_handle_t>(_rmtChannel);
	auto const encoder = static_cast<rmt_encoder_handle_t>(_rmtEncoder);

	std::array<rmt_symbol_word_t, samsungFrameSymbolCount> symbols{};
	std::size_t index = 0;
	symbols[index++] = symbol(samsungHeaderMarkUs, samsungHeaderSpaceUs);
	for (std::uint8_t bit = 0; bit < 32U; ++bit) {
		auto const mask = static_cast<std::uint32_t>(1UL << (31U - bit));
		symbols[index++] = symbol(samsungBitMarkUs, (code & mask) != 0 ? samsungOneSpaceUs : samsungZeroSpaceUs);
	}
	symbols[index++] = symbol(samsungBitMarkUs, 0);

	rmt_transmit_config_t transmitConfig{};
	transmitConfig.loop_count = 0;
	transmitConfig.flags.eot_level = 0;
	if (rmt_transmit(channel, encoder, symbols.data(), symbols.size() * sizeof(symbols[0]), &transmitConfig) != ESP_OK) {
		return false;
	}
	return rmt_tx_wait_all_done(channel, 200) == ESP_OK;
}

} // namespace lexxos::devices
