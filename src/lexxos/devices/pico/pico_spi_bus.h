#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <hardware/dma.h>
#include <hardware/spi.h>
#include <pico/sem.h>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/pico/detail/spi_bus_guard.h>
#include <lexxos/devices/pico/detail/spi_bus_mutex.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices {

class PicoSpiBus final : public SpiBus {
public:
	using SpiBus::SpiBus;

	~PicoSpiBus() override;

	bool initialize(lexxos::platform::SpiPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool setBaudRate(std::uint32_t baudRate) override;
	bool setClockMode(SpiClockMode mode) override;
	[[nodiscard]] SpiClockMode clockMode() const override;
	void select() override;
	void deselect() override;
	bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool write(std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) override;
	[[nodiscard]] bool supportsDma() const override;
	bool startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool waitDmaTransfer(std::uint32_t timeoutMs) override;
	bool beginTransaction() override;
	void endTransaction() override;

	bool handleDmaInterrupt(std::uint32_t irqIndex);

private:
	void releaseDmaBusLock();
	void applyClockMode(SpiClockMode mode);
	bool ensureDmaReady();
	void configureDmaDreq();
	void clearDmaInterruptStatus();
	[[nodiscard]] bool spiIsFaulted() const;

	spi_inst_t* _instance{nullptr};
	lexxos::platform::SpiPins _pins;
	SpiClockMode _mode{SpiClockMode::Mode0};
	bool _ready{false};
	bool _dmaReady{false};
	bool _dmaTransferActive{false};
	bool _dmaBusLocked{false};
	std::uint32_t _selectLockDepth{0};
	int _txDmaChannel{-1};
	int _rxDmaChannel{-1};
	dma_channel_config _txDmaConfig{};
	dma_channel_config _rxDmaConfig{};
	semaphore_t _dmaDone{};
	std::uint8_t _dmaFill{0xFF};
	std::uint8_t _dmaDiscard{0};
	SpiBusMutex _mutex;
};

} // namespace lexxos::devices
