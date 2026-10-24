#include <lexxos/devices/pico/pico_spi_bus.h>

#include <array>

#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <pico/platform.h>

#ifndef LEXXOS_PICO_SPI_FAST_PADS
	#define LEXXOS_PICO_SPI_FAST_PADS 0
#endif

namespace lexxos::devices {
namespace {

	constexpr std::size_t spiDmaOwnerSlots = 4;
	std::array<PicoSpiBus*, spiDmaOwnerSlots> g_dmaOwners{};
	bool g_dmaIrqHandlerInstalled{false};

	spi_inst_t* toPicoSpi(int index) {
		return index == 1 ? spi1 : spi0;
	}

	uint toPicoGpioPin(lexxos::platform::GpioPin pin) {
		return static_cast<uint>(pin.number);
	}

	spi_cpol_t cpolForMode(SpiClockMode mode) {
		return (static_cast<std::uint8_t>(mode) & 0x02U) != 0 ? SPI_CPOL_1 : SPI_CPOL_0;
	}

	spi_cpha_t cphaForMode(SpiClockMode mode) {
		return (static_cast<std::uint8_t>(mode) & 0x01U) != 0 ? SPI_CPHA_1 : SPI_CPHA_0;
	}

	void configureOutputPad(lexxos::platform::GpioPin pin) {
#if LEXXOS_PICO_SPI_FAST_PADS
		if (!pin.isValid()) {
			return;
		}
		uint const gpio = toPicoGpioPin(pin);
		gpio_set_drive_strength(gpio, GPIO_DRIVE_STRENGTH_12MA);
		gpio_set_slew_rate(gpio, GPIO_SLEW_RATE_FAST);
#else
		(void) pin;
#endif
	}

	void configureInputPad(lexxos::platform::GpioPin pin) {
		if (!pin.isValid()) {
			return;
		}

		// Подтяжка MISO нужна и в консервативном профиле: когда карта не передаёт
		// данные, линия должна оставаться в состоянии 0xFF во время CMD0 и ACMD41.
		gpio_pull_up(toPicoGpioPin(pin));
	}

	void addDmaOwner(PicoSpiBus& bus);
	void removeDmaOwner(PicoSpiBus& bus);
	void spiDmaIrqHandler0();

} // namespace

PicoSpiBus::~PicoSpiBus() {
	removeDmaOwner(*this);
}

bool PicoSpiBus::initialize(lexxos::platform::SpiPins const& pins) {
	if (!pins.isValid() || !_mutex.initialize()) {
		_ready = false;
		_instance = nullptr;
		return false;
	}

	_pins = pins;
	_instance = toPicoSpi(pins.index);
	_dmaTransferActive = false;
	_dmaBusLocked = false;
	_selectLockDepth = 0;

	// Повторная инициализация всегда начинается с базового для SD-карты Mode0.
	_mode = SpiClockMode::Mode0;

	// Карта должна быть снята с выбора до передачи SCK/MOSI периферии SPI.
	gpio_init(toPicoGpioPin(pins.cs));
	gpio_set_dir(toPicoGpioPin(pins.cs), GPIO_OUT);
	gpio_put(toPicoGpioPin(pins.cs), 1);
	configureOutputPad(pins.cs);

	spi_init(_instance, pins.baudRate);
	applyClockMode(_mode);

	gpio_set_function(toPicoGpioPin(pins.sck), GPIO_FUNC_SPI);
	gpio_set_function(toPicoGpioPin(pins.mosi), GPIO_FUNC_SPI);
	gpio_set_function(toPicoGpioPin(pins.miso), GPIO_FUNC_SPI);
	configureOutputPad(pins.sck);
	configureOutputPad(pins.mosi);
	configureInputPad(pins.miso);

	gpio_put(toPicoGpioPin(pins.cs), 1);
	_ready = true;

	(void) ensureDmaReady();
	return true;
}

bool PicoSpiBus::isReady() const {
	return _ready && _instance != nullptr;
}

bool PicoSpiBus::setBaudRate(std::uint32_t baudRate) {
	if (!isReady()) {
		return false;
	}

	SpiBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	spi_set_baudrate(_instance, baudRate);
	_pins.baudRate = baudRate;
	return true;
}

bool PicoSpiBus::setClockMode(SpiClockMode mode) {
	if (!isReady()) {
		_mode = mode;
		return true;
	}

	SpiBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	_mode = mode;
	applyClockMode(_mode);
	return true;
}

SpiClockMode PicoSpiBus::clockMode() const {
	return _mode;
}

void PicoSpiBus::select() {
	if (!isReady() || !_mutex.lock()) {
		return;
	}
	++_selectLockDepth;
	if (_pins.cs.isValid()) {
		gpio_put(toPicoGpioPin(_pins.cs), 0);
	}
}

void PicoSpiBus::deselect() {
	if (!isReady()) {
		return;
	}
	if (_pins.cs.isValid()) {
		gpio_put(toPicoGpioPin(_pins.cs), 1);
	}
	if (_selectLockDepth == 0) {
		return;
	}
	--_selectLockDepth;
	_mutex.unlock();
}

bool PicoSpiBus::transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (!isReady()) {
		return false;
	}

	SpiBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	return spi_write_read_blocking(_instance, tx, rx, size) == static_cast<int>(size);
}

bool PicoSpiBus::write(std::uint8_t const* data, std::size_t size) {
	if (!isReady()) {
		return false;
	}

	SpiBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	return spi_write_blocking(_instance, data, size) == static_cast<int>(size);
}

bool PicoSpiBus::read(std::uint8_t fill, std::uint8_t* data, std::size_t size) {
	if (!isReady()) {
		return false;
	}

	SpiBusGuard guard{_mutex};
	if (!guard.locked()) {
		return false;
	}

	return spi_read_blocking(_instance, fill, data, size) == static_cast<int>(size);
}

bool PicoSpiBus::supportsDma() const {
	return _dmaReady;
}

bool PicoSpiBus::startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
	if (!isReady() || size == 0) {
		return size == 0;
	}
	if (!_mutex.lock()) {
		return false;
	}
	if (!ensureDmaReady()) {
		bool const result = SpiBus::startDmaTransfer(tx, rx, size);
		_mutex.unlock();
		return result;
	}
	if (_dmaTransferActive) {
		_mutex.unlock();
		return false;
	}

	std::uint8_t const* txSource = tx;
	std::uint8_t* rxTarget = rx;
	if (txSource == nullptr) {
		txSource = &_dmaFill;
	}
	if (rxTarget == nullptr) {
		rxTarget = &_dmaDiscard;
	}

	channel_config_set_read_increment(&_txDmaConfig, tx != nullptr);
	channel_config_set_write_increment(&_txDmaConfig, false);
	channel_config_set_read_increment(&_rxDmaConfig, false);
	channel_config_set_write_increment(&_rxDmaConfig, rx != nullptr);

	dma_channel_configure(_txDmaChannel,
	    &_txDmaConfig,
	    &spi_get_hw(_instance)->dr,
	    txSource,
	    size,
	    false);
	dma_channel_configure(_rxDmaChannel,
	    &_rxDmaConfig,
	    rxTarget,
	    &spi_get_hw(_instance)->dr,
	    size,
	    false);

	clearDmaInterruptStatus();
	sem_reset(&_dmaDone, 0);
	_dmaTransferActive = true;
	_dmaBusLocked = true;
	dma_start_channel_mask((1U << _txDmaChannel) | (1U << _rxDmaChannel));
	return true;
}

bool PicoSpiBus::waitDmaTransfer(std::uint32_t timeoutMs) {
	if (!_dmaTransferActive) {
		releaseDmaBusLock();
		return true;
	}

	bool const completed = sem_acquire_timeout_ms(&_dmaDone, timeoutMs);
	if (!completed) {
		dma_channel_abort(_txDmaChannel);
		dma_channel_abort(_rxDmaChannel);
		clearDmaInterruptStatus();
		_dmaTransferActive = false;
		releaseDmaBusLock();
		return false;
	}

	dma_channel_wait_for_finish_blocking(_txDmaChannel);
	dma_channel_wait_for_finish_blocking(_rxDmaChannel);
	_dmaTransferActive = false;
	bool const result = !dma_channel_is_busy(_txDmaChannel) && !dma_channel_is_busy(_rxDmaChannel) && !spiIsFaulted();
	releaseDmaBusLock();
	return result;
}

bool PicoSpiBus::beginTransaction() {
	return isReady() && _mutex.lock();
}

void PicoSpiBus::endTransaction() {
	_mutex.unlock();
}

bool PicoSpiBus::handleDmaInterrupt(std::uint32_t irqIndex) {
	if (!_dmaReady || _rxDmaChannel < 0) {
		return false;
	}

	io_rw_32* status = irqIndex == 1 ? &dma_hw->ints1 : &dma_hw->ints0;
	std::uint32_t const mask = 1U << _rxDmaChannel;
	if ((*status & mask) == 0) {
		return false;
	}

	*status = mask;
	sem_release(&_dmaDone);
	return true;
}

void PicoSpiBus::releaseDmaBusLock() {
	if (!_dmaBusLocked) {
		return;
	}
	_dmaBusLocked = false;
	_mutex.unlock();
}

void PicoSpiBus::applyClockMode(SpiClockMode mode) {
	spi_set_format(_instance, 8, cpolForMode(mode), cphaForMode(mode), SPI_MSB_FIRST);
}

bool PicoSpiBus::ensureDmaReady() {
	if (_dmaReady) {
		configureDmaDreq();
		return true;
	}
	if (!isReady()) {
		return false;
	}

	_txDmaChannel = dma_claim_unused_channel(false);
	_rxDmaChannel = dma_claim_unused_channel(false);
	if (_txDmaChannel < 0 || _rxDmaChannel < 0) {
		if (_txDmaChannel >= 0) {
			dma_channel_unclaim(_txDmaChannel);
		}
		if (_rxDmaChannel >= 0) {
			dma_channel_unclaim(_rxDmaChannel);
		}
		_txDmaChannel = -1;
		_rxDmaChannel = -1;
		return false;
	}

	_txDmaConfig = dma_channel_get_default_config(_txDmaChannel);
	_rxDmaConfig = dma_channel_get_default_config(_rxDmaChannel);
	channel_config_set_transfer_data_size(&_txDmaConfig, DMA_SIZE_8);
	channel_config_set_transfer_data_size(&_rxDmaConfig, DMA_SIZE_8);
	configureDmaDreq();

	sem_init(&_dmaDone, 0, 1);
	dma_channel_set_irq0_enabled(_rxDmaChannel, true);
	dma_channel_set_irq0_enabled(_txDmaChannel, false);
	if (!g_dmaIrqHandlerInstalled) {
		irq_add_shared_handler(DMA_IRQ_0, spiDmaIrqHandler0, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
		irq_set_enabled(DMA_IRQ_0, true);
		g_dmaIrqHandlerInstalled = true;
	}
	addDmaOwner(*this);
	_dmaReady = true;
	return true;
}

void PicoSpiBus::configureDmaDreq() {
	if (_instance == nullptr || _txDmaChannel < 0 || _rxDmaChannel < 0) {
		return;
	}

	int const spiIndex = spi_get_index(_instance);
	channel_config_set_dreq(&_txDmaConfig, spiIndex == 1 ? DREQ_SPI1_TX : DREQ_SPI0_TX);
	channel_config_set_dreq(&_rxDmaConfig, spiIndex == 1 ? DREQ_SPI1_RX : DREQ_SPI0_RX);
}

void PicoSpiBus::clearDmaInterruptStatus() {
	if (_rxDmaChannel < 0) {
		return;
	}
	dma_hw->ints0 = 1U << _rxDmaChannel;
}

bool PicoSpiBus::spiIsFaulted() const {
	if (_instance == nullptr) {
		return true;
	}
	auto const* hw = spi_get_const_hw(_instance);
	return (hw->sr & SPI_SSPSR_RFF_BITS) != 0;
}

namespace {

	void addDmaOwner(PicoSpiBus& bus) {
		for (auto* owner : g_dmaOwners) {
			if (owner == &bus) {
				return;
			}
		}
		for (auto& owner : g_dmaOwners) {
			if (owner == nullptr) {
				owner = &bus;
				return;
			}
		}
	}

	void removeDmaOwner(PicoSpiBus& bus) {
		for (auto& owner : g_dmaOwners) {
			if (owner == &bus) {
				owner = nullptr;
			}
		}
	}

	void handleDmaIrq(std::uint32_t irqIndex) {
		for (auto* owner : g_dmaOwners) {
			if (owner != nullptr && owner->handleDmaInterrupt(irqIndex)) {
				return;
			}
		}
	}

	void __not_in_flash_func(spiDmaIrqHandler0)() {
		handleDmaIrq(0);
	}

} // namespace
} // namespace lexxos::devices
