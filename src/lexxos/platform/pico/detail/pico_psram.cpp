#include <lexxos/platform/platform.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/pio_instructions.h>
#include <hardware/structs/sio.h>
#include <pico/time.h>

namespace {

constexpr std::uint8_t psramReadCommand = 0x03;
constexpr std::uint8_t psramWriteCommand = 0x02;
constexpr std::uint32_t psramCapacityBytes = 0x01000000U;
constexpr std::uint32_t psramPreferredTransferBytes = 4096;
constexpr std::uint32_t pioTransferThresholdHz = 1;
constexpr std::uint32_t psramFallbackBaudRate = 8000000;
constexpr std::uint32_t minPioClockDividerX1000 = 1000;
constexpr std::uint32_t pioTransferTimeoutSlackMs = 250;
constexpr float pioCyclesPerBit = 2.0F;

struct PsramBus {
	std::uint32_t csMask{0};
	std::uint32_t sckMask{0};
	std::uint32_t mosiMask{0};
	std::uint32_t misoMask{0};
	std::uint32_t delayUs{0};
};

struct PsramPioEngine {
	PIO pio{nullptr};
	int sm{-1};
	unsigned int offset{0};
	bool programLoaded{false};
	bool configured{false};
	std::uint8_t cs{0};
	std::uint8_t sck{0};
	std::uint8_t mosi{0};
	std::uint8_t miso{0};
	std::uint32_t baudRate{0};
	std::uint32_t csMask{0};
	std::uint32_t sckMask{0};
};

struct PsramRuntimeStats {
	char const* backend{"none"};
	char const* lastPioFailure{"none"};
	std::uint32_t transferCount{0};
	std::uint32_t pioTransferCount{0};
	std::uint32_t softwareTransferCount{0};
	std::uint32_t pioRejectedLowBaudCount{0};
	std::uint32_t pioConfigureFailureCount{0};
	std::uint32_t pioTransferTimeoutCount{0};
	std::uint32_t lastTransferBytes{0};
};

// SPI mode 0, MSB first, one bit per two PIO cycles.
// This is the last hardware-confirmed stable PicoCalc PSRAM PIO waveform at
// 8 MHz. The four-instruction delayed-sample waveform timed out on RP2350
// because the RX FIFO was not filled reliably, so keep this conservative path
// as the default ROM-staging backend.
//   OUT PINS, 1  SIDE 0   ; prepare MOSI while SCK is low
//   IN  PINS, 1  SIDE 1   ; sample MISO while SCK is already high
std::uint16_t const psramPioInstructions[] = {
    static_cast<std::uint16_t>(pio_encode_out(pio_pins, 1) | pio_encode_sideset(1, 0)),
    static_cast<std::uint16_t>(pio_encode_in(pio_pins, 1) | pio_encode_sideset(1, 1)),
};

pio_program const psramPioProgram{
    .instructions = psramPioInstructions,
    .length = 2,
    .origin = -1,
};

[[nodiscard]] bool gpioMask(lexxos::platform::GpioPin pin, std::uint32_t& output) {
	if (!pin.isValid() || pin.number < 0 || pin.number >= 32) {
		return false;
	}
	output = 1UL << static_cast<unsigned>(pin.number);
	return true;
}

[[nodiscard]] bool gpioNumber(lexxos::platform::GpioPin pin, std::uint8_t& output) {
	if (!pin.isValid() || pin.number < 0 || pin.number >= 32) {
		return false;
	}
	output = static_cast<std::uint8_t>(pin.number);
	return true;
}

void configureOutput(lexxos::platform::GpioPin pin, bool level) {
	auto const gpio = static_cast<unsigned>(pin.number);
	gpio_init(gpio);
	gpio_set_function(gpio, GPIO_FUNC_SIO);
	gpio_set_dir(gpio, GPIO_OUT);
	gpio_put(gpio, level ? 1 : 0);
}

void configureInput(lexxos::platform::GpioPin pin) {
	auto const gpio = static_cast<unsigned>(pin.number);
	gpio_init(gpio);
	gpio_set_function(gpio, GPIO_FUNC_SIO);
	gpio_set_dir(gpio, GPIO_IN);
	gpio_disable_pulls(gpio);
}

[[nodiscard]] bool configureBus(lexxos::platform::PsramPins const& pins, PsramBus& bus) {
	if (!pins.spi.isValid()) {
		return false;
	}
	if (!gpioMask(pins.spi.cs, bus.csMask)
	    || !gpioMask(pins.spi.sck, bus.sckMask)
	    || !gpioMask(pins.spi.mosi, bus.mosiMask)
	    || !gpioMask(pins.spi.miso, bus.misoMask)) {
		return false;
	}

	configureOutput(pins.spi.cs, true);
	configureOutput(pins.spi.sck, false);
	configureOutput(pins.spi.mosi, false);
	if (pins.sio2.isValid()) {
		configureOutput(pins.sio2, true);
	}
	if (pins.sio3.isValid()) {
		configureOutput(pins.sio3, true);
	}
	configureInput(pins.spi.miso);

	// The software fallback is a diagnostic safety net, not a fast path. Keep it
	// deliberately slow and stable so it cannot silently corrupt ROM staging data
	// if PIO is unavailable.
	auto const baudRate = pins.spi.baudRate == 0 ? psramFallbackBaudRate : pins.spi.baudRate;
	bus.delayUs = baudRate == 0 ? 1 : std::max<std::uint32_t>(1, (500000U + baudRate - 1U) / baudRate);
	return true;
}

inline void setMask(std::uint32_t mask) {
	sio_hw->gpio_set = mask;
}

inline void clearMask(std::uint32_t mask) {
	sio_hw->gpio_clr = mask;
}

inline bool readMask(std::uint32_t mask) {
	return (sio_hw->gpio_in & mask) != 0;
}

inline void bitDelay(PsramBus const& bus) {
	if (bus.delayUs != 0) {
		busy_wait_us_32(bus.delayUs);
	}
}

std::uint8_t transferByte(PsramBus const& bus, std::uint8_t value) {
	std::uint8_t result = 0;
	for (std::uint8_t bit = 0; bit < 8; ++bit) {
		if ((value & 0x80U) != 0) {
			setMask(bus.mosiMask);
		} else {
			clearMask(bus.mosiMask);
		}
		bitDelay(bus);
		setMask(bus.sckMask);
		bitDelay(bus);
		result = static_cast<std::uint8_t>((result << 1U) | (readMask(bus.misoMask) ? 1U : 0U));
		clearMask(bus.sckMask);
		bitDelay(bus);
		value = static_cast<std::uint8_t>(value << 1U);
	}
	return result;
}

void select(PsramBus const& bus) {
	clearMask(bus.sckMask);
	setMask(bus.csMask);
	bitDelay(bus);
	clearMask(bus.csMask);
	bitDelay(bus);
}

void deselect(PsramBus const& bus) {
	clearMask(bus.sckMask);
	setMask(bus.csMask);
	bitDelay(bus);
}

void transferAddress(PsramBus const& bus, std::uint32_t address) {
	(void) transferByte(bus, static_cast<std::uint8_t>((address >> 16U) & 0xFFU));
	(void) transferByte(bus, static_cast<std::uint8_t>((address >> 8U) & 0xFFU));
	(void) transferByte(bus, static_cast<std::uint8_t>(address & 0xFFU));
}

[[nodiscard]] std::uint32_t effectiveBaudRate(lexxos::platform::PsramPins const& pins) {
	return pins.spi.baudRate == 0 ? psramFallbackBaudRate : pins.spi.baudRate;
}

[[nodiscard]] bool shouldUsePio(lexxos::platform::PsramPins const& pins) {
	return effectiveBaudRate(pins) >= pioTransferThresholdHz;
}

PsramPioEngine& pioEngine() {
	static PsramPioEngine engine{};
	return engine;
}

PsramRuntimeStats& runtimeStats() {
	static PsramRuntimeStats stats{};
	return stats;
}

[[nodiscard]] int pioIndex(PIO pio) {
	if (pio == pio0) {
		return 0;
	}
	if (pio == pio1) {
		return 1;
	}
	return -1;
}

[[nodiscard]] bool pioPinsStillConfigured(PsramPioEngine const& engine) {
	if (!engine.configured || engine.pio == nullptr || engine.sm < 0) {
		return false;
	}

	auto const expectedFunction = engine.pio == pio0 ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1;
	return gpio_get_function(engine.sck) == expectedFunction
	    && gpio_get_function(engine.mosi) == expectedFunction
	    && gpio_get_function(engine.miso) == expectedFunction;
}

[[nodiscard]] std::uint32_t pioTransferTimeoutMs(std::size_t size, std::uint32_t baudRate) {
	auto const effectiveBaud = baudRate == 0 ? psramFallbackBaudRate : baudRate;
	if (effectiveBaud == 0) {
		return pioTransferTimeoutSlackMs;
	}
	auto const limitedSize = std::min<std::size_t>(size, 0x1FFFFFFFU);
	auto const bitCount = static_cast<std::uint64_t>(limitedSize) * 8ULL;
	auto const expectedMs = (bitCount * 1000ULL + static_cast<std::uint64_t>(effectiveBaud) - 1ULL)
	    / static_cast<std::uint64_t>(effectiveBaud);
	auto const timeoutMs = expectedMs + static_cast<std::uint64_t>(pioTransferTimeoutSlackMs);
	if (timeoutMs > 5000ULL) {
		return 5000;
	}
	return std::max<std::uint32_t>(pioTransferTimeoutSlackMs, static_cast<std::uint32_t>(timeoutMs));
}

void abortPioEngine(PsramPioEngine& engine) {
	if (engine.pio == nullptr || engine.sm < 0) {
		return;
	}
	pio_sm_set_enabled(engine.pio, static_cast<unsigned>(engine.sm), false);
	pio_sm_clear_fifos(engine.pio, static_cast<unsigned>(engine.sm));
	pio_sm_restart(engine.pio, static_cast<unsigned>(engine.sm));
	if (engine.csMask != 0) {
		setMask(engine.csMask);
	}

	engine.configured = false;
}

[[nodiscard]] bool claimPioEngine(PsramPioEngine& engine) {
	if (engine.programLoaded) {
		return true;
	}

	PIO candidates[] = {pio0, pio1};
	for (auto* pio : candidates) {
		if (!pio_can_add_program(pio, &psramPioProgram)) {
			continue;
		}
		auto const sm = pio_claim_unused_sm(pio, false);
		if (sm < 0) {
			continue;
		}
		engine.pio = pio;
		engine.sm = sm;
		engine.offset = pio_add_program(pio, &psramPioProgram);
		engine.programLoaded = true;
		return true;
	}
	return false;
}

void setPioClockLow(PsramPioEngine const& engine) {
	if (engine.pio == nullptr || engine.sm < 0) {
		return;
	}
	pio_sm_exec_wait_blocking(
	    engine.pio,
	    static_cast<unsigned>(engine.sm),
	    static_cast<std::uint16_t>(pio_encode_nop() | pio_encode_sideset(1, 0)));
}

[[nodiscard]] bool configurePioEngine(lexxos::platform::PsramPins const& pins, PsramPioEngine& engine) {
	std::uint8_t cs = 0;
	std::uint8_t sck = 0;
	std::uint8_t mosi = 0;
	std::uint8_t miso = 0;
	std::uint32_t csMask = 0;
	std::uint32_t sckMask = 0;
	// PicoCalc PSRAM is intentionally described with spi.index = -1 because
	// these GPIOs are not a valid RP2xxx hardware-SPI pin group. The PIO path
	// only needs valid GPIO numbers, so do not reject the board on spi.index.
	if (!gpioNumber(pins.spi.cs, cs)
	    || !gpioNumber(pins.spi.sck, sck)
	    || !gpioNumber(pins.spi.mosi, mosi)
	    || !gpioNumber(pins.spi.miso, miso)
	    || !gpioMask(pins.spi.cs, csMask)
	    || !gpioMask(pins.spi.sck, sckMask)) {
		return false;
	}
	if (!claimPioEngine(engine)) {
		return false;
	}

	auto const baudRate = effectiveBaudRate(pins);
	if (engine.configured
	    && engine.cs == cs
	    && engine.sck == sck
	    && engine.mosi == mosi
	    && engine.miso == miso
	    && engine.baudRate == baudRate
	    && pioPinsStillConfigured(engine)) {
		return true;
	}

	configureOutput(pins.spi.cs, true);
	if (pins.sio2.isValid()) {
		configureOutput(pins.sio2, true);
	}
	if (pins.sio3.isValid()) {
		configureOutput(pins.sio3, true);
	}

	pio_gpio_init(engine.pio, sck);
	pio_gpio_init(engine.pio, mosi);
	pio_gpio_init(engine.pio, miso);
	gpio_disable_pulls(miso);

	pio_sm_set_enabled(engine.pio, static_cast<unsigned>(engine.sm), false);
	pio_sm_clear_fifos(engine.pio, static_cast<unsigned>(engine.sm));
	pio_sm_restart(engine.pio, static_cast<unsigned>(engine.sm));
	pio_sm_set_consecutive_pindirs(engine.pio, static_cast<unsigned>(engine.sm), sck, 1, true);
	pio_sm_set_consecutive_pindirs(engine.pio, static_cast<unsigned>(engine.sm), mosi, 1, true);
	pio_sm_set_consecutive_pindirs(engine.pio, static_cast<unsigned>(engine.sm), miso, 1, false);

	auto config = pio_get_default_sm_config();
	sm_config_set_wrap(&config, engine.offset, engine.offset + psramPioProgram.length - 1U);
	sm_config_set_sideset(&config, 1, false, false);
	sm_config_set_sideset_pins(&config, sck);
	sm_config_set_out_pins(&config, mosi, 1);
	sm_config_set_in_pins(&config, miso);
	sm_config_set_out_shift(&config, false, true, 8);
	sm_config_set_in_shift(&config, false, true, 8);

	float divider = static_cast<float>(clock_get_hz(clk_sys))
	    / (static_cast<float>(baudRate) * pioCyclesPerBit);
	if (divider < static_cast<float>(minPioClockDividerX1000) / 1000.0F) {
		divider = static_cast<float>(minPioClockDividerX1000) / 1000.0F;
	}
	sm_config_set_clkdiv(&config, divider);

	pio_sm_init(engine.pio, static_cast<unsigned>(engine.sm), engine.offset, &config);
	pio_sm_set_enabled(engine.pio, static_cast<unsigned>(engine.sm), true);
	setPioClockLow(engine);

	engine.configured = true;
	engine.cs = cs;
	engine.sck = sck;
	engine.mosi = mosi;
	engine.miso = miso;
	engine.baudRate = baudRate;
	engine.csMask = csMask;
	engine.sckMask = sckMask;
	return true;
}

[[nodiscard]] bool transferPioBytes(
    PsramPioEngine const& engine, std::uint8_t const* input, std::uint8_t* output, std::size_t size, std::uint32_t timeoutMs) {
	std::size_t written = 0;
	std::size_t read = 0;
	auto const sm = static_cast<unsigned>(engine.sm);
	auto progressAtMs = to_ms_since_boot(get_absolute_time());

	while (read < size) {
		bool progressed = false;
		while (written < size && !pio_sm_is_tx_fifo_full(engine.pio, sm)) {
			auto const value = input == nullptr ? 0xFFU : input[written];
			pio_sm_put(engine.pio, sm, static_cast<std::uint32_t>(value) << 24U);
			++written;
			progressed = true;
		}

		if (!pio_sm_is_rx_fifo_empty(engine.pio, sm)) {
			auto const value = static_cast<std::uint8_t>(pio_sm_get(engine.pio, sm) & 0xFFU);
			if (output != nullptr) {
				output[read] = value;
			}
			++read;
			progressed = true;
		}

		if (progressed) {
			progressAtMs = to_ms_since_boot(get_absolute_time());
		} else {
			auto const nowMs = to_ms_since_boot(get_absolute_time());
			if (nowMs - progressAtMs >= timeoutMs) {
				return false;
			}

			tight_loop_contents();
		}
	}
	return true;
}

[[nodiscard]] bool transferPioMemory(std::uint32_t offset, void const* writeData, void* readData, std::size_t size) {
	if (size == 0) {
		return true;
	}

	auto const& pins = lexxos::platform::activeBoardDescriptor().psram;
	auto& stats = runtimeStats();

	if (!shouldUsePio(pins)) {
		++stats.pioRejectedLowBaudCount;
		stats.lastPioFailure = "baud-below-threshold";
		return false;
	}

	auto& engine = pioEngine();
	if (!configurePioEngine(pins, engine)) {
		++stats.pioConfigureFailureCount;
		stats.lastPioFailure = "configure-failed";
		return false;
	}

	stats.lastPioFailure = "none";

	std::uint8_t const header[] = {
	    static_cast<std::uint8_t>(writeData == nullptr ? psramReadCommand : psramWriteCommand),
	    static_cast<std::uint8_t>((offset >> 16U) & 0xFFU),
	    static_cast<std::uint8_t>((offset >> 8U) & 0xFFU),
	    static_cast<std::uint8_t>(offset & 0xFFU),
	};

	pio_sm_set_enabled(engine.pio, static_cast<unsigned>(engine.sm), false);
	pio_sm_clear_fifos(engine.pio, static_cast<unsigned>(engine.sm));
	pio_sm_restart(engine.pio, static_cast<unsigned>(engine.sm));
	pio_sm_set_enabled(engine.pio, static_cast<unsigned>(engine.sm), true);
	setPioClockLow(engine);
	setMask(engine.csMask);
	clearMask(engine.csMask);
	auto const headerTimeoutMs = pioTransferTimeoutMs(sizeof(header), effectiveBaudRate(pins));
	auto const dataTimeoutMs = pioTransferTimeoutMs(size, effectiveBaudRate(pins));
	if (!transferPioBytes(engine, header, nullptr, sizeof(header), headerTimeoutMs)
	    || !transferPioBytes(
	        engine,
	        static_cast<std::uint8_t const*>(writeData),
	        static_cast<std::uint8_t*>(readData),
	        size,
	        dataTimeoutMs)) {
		++stats.pioTransferTimeoutCount;
		stats.lastPioFailure = "transfer-timeout";
		abortPioEngine(engine);
		return false;
	}
	setPioClockLow(engine);
	setMask(engine.csMask);
	return true;
}

[[nodiscard]] bool rangeIsValid(std::uint32_t offset, std::size_t size) {
	return size == 0 || (offset < psramCapacityBytes && size <= psramCapacityBytes - offset);
}

[[nodiscard]] bool transferSoftwareMemory(std::uint32_t offset, void const* writeData, void* readData, std::size_t size) {
	if (size == 0) {
		return true;
	}

	PsramBus bus{};
	auto const& pins = lexxos::platform::activeBoardDescriptor().psram;
	if (!configureBus(pins, bus)) {
		return false;
	}

	auto const* input = static_cast<std::uint8_t const*>(writeData);
	auto* output = static_cast<std::uint8_t*>(readData);
	select(bus);
	(void) transferByte(bus, input == nullptr ? psramReadCommand : psramWriteCommand);
	transferAddress(bus, offset);
	for (std::size_t index = 0; index < size; ++index) {
		if (input != nullptr) {
			(void) transferByte(bus, input[index]);
		} else {
			output[index] = transferByte(bus, 0xFF);
		}
	}
	deselect(bus);
	return true;
}

[[nodiscard]] bool transferMemory(std::uint32_t offset, void const* writeData, void* readData, std::size_t size) {
	if (!rangeIsValid(offset, size) || (writeData == nullptr && readData == nullptr)) {
		return false;
	}

	auto& stats = runtimeStats();
	auto const& pins = lexxos::platform::activeBoardDescriptor().psram;

	++stats.transferCount;
	stats.lastTransferBytes = static_cast<std::uint32_t>(std::min<std::size_t>(size, 0xFFFFFFFFU));
	if (size == 0) {
		stats.backend = "none";
		return true;
	}

	if (shouldUsePio(pins)) {
		if (transferPioMemory(offset, writeData, readData, size)) {
			++stats.pioTransferCount;
			stats.backend = "pio";
			return true;
		}
		// Do not silently fall back to the software bit-bang path after a PIO
		// failure. A failed PIO transfer means the ROM staging path is not
		// trustworthy; continuing with a different backend can corrupt PSRAM and
		// later crash gbemu with bogus opcodes.
		stats.backend = "pio-failed";
		return false;
	}

	// Only use software SPI when PIO was not requested by the active board
	// configuration. This remains useful for very slow bring-up diagnostics.
	auto const ok = transferSoftwareMemory(offset, writeData, readData, size);
	++stats.softwareTransferCount;
	stats.backend = ok ? "software" : "software-failed";
	return ok;
}

} // namespace

bool lexxos::platform::gameBoyRomPsramRegion(GameBoyRomExternalMemoryRegion& output) {
	output = {};
	if (!activeBoardDescriptor().psram.spi.isValid()) {
		return false;
	}
	output.available = true;
	output.capacity = psramCapacityBytes;
	output.preferredTransferBytes = psramPreferredTransferBytes;
	return true;
}

bool lexxos::platform::gameBoyRomPsramRuntimeInfo(GameBoyRomPsramRuntimeInfo& output) {
	output = {};
	auto const& pins = activeBoardDescriptor().psram;
	auto const& stats = runtimeStats();
	auto const& engine = pioEngine();
	output.available = pins.spi.isValid();
	output.configuredBaudRate = pins.spi.baudRate;
	output.effectiveBaudRate = effectiveBaudRate(pins);
	output.backend = stats.backend;
	output.lastPioFailure = stats.lastPioFailure;
	output.pioConfigured = engine.configured;
	output.pioIndex = pioIndex(engine.pio);
	output.stateMachine = engine.sm;
	output.transferCount = stats.transferCount;
	output.pioTransferCount = stats.pioTransferCount;
	output.softwareTransferCount = stats.softwareTransferCount;
	output.pioRejectedLowBaudCount = stats.pioRejectedLowBaudCount;
	output.pioConfigureFailureCount = stats.pioConfigureFailureCount;
	output.pioTransferTimeoutCount = stats.pioTransferTimeoutCount;
	output.lastTransferBytes = stats.lastTransferBytes;
	return output.available;
}

bool lexxos::platform::readGameBoyRomPsram(std::uint32_t offset, void* data, std::size_t size) {
	return data != nullptr && transferMemory(offset, nullptr, data, size);
}

bool lexxos::platform::writeGameBoyRomPsram(std::uint32_t offset, void const* data, std::size_t size) {
	return data != nullptr && transferMemory(offset, data, nullptr, size);
}
