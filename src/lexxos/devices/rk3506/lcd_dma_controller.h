#pragma once

#include <lexxos/devices/rk3506/lcd_spi_dma.h>
#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
#include <lexxos/drivers/core/dma/pl330_tx_program.h>

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::platform::rk3506 {

// Boot-lifetime, non-copyable owner for SPI0's LCD DMA channel. Composition binds
// exactly one instance to the IRQ/legacy adapter before probe.
class LcdDmaController final : public LcdSpiDma {
public:
	LcdDmaController();
	LcdDmaController(LcdDmaController const&) = delete;
	LcdDmaController& operator=(LcdDmaController const&) = delete;
	void setWaitFunction(void (*wait)(std::uint32_t));
	bool probe();
	bool setEnabled(bool enabled);
	bool enabled() override;
	bool prepareRamReload();
	bool registerSource(std::uint8_t const* source, std::size_t capacity);
	bool write(std::uint8_t const* source, std::size_t count) override;
	bool write16(std::uint16_t const* source, std::size_t pixels) override;
	char const* reason();
	LcdDmaStats statistics();
	LcdDmaFailure lastFailure();
	LcdDmaRegisters registerSnapshot();
	bool handleIrq(std::uint32_t id);

	bool sourceRetained() const override { return _active; }

	bool quarantined() const { return _quarantined; }

private:
	friend bool lcdDmaBindController(LcdDmaController& controller);
	void releaseOwnedSourceIfStopped();
	bool buildCode(std::uint8_t const* source, std::uint32_t size, std::uint32_t unitBytes, std::uint8_t burstUnits);

#if defined(LEXXOS_RK3506_DMA_SAFE)
	bool prepareDmaBuffers();
#endif

	void captureFailure(char const* stage);

	bool fail(char const* reason, bool timeout = false);
	bool transmitChunk(std::uint8_t const* data, std::uint32_t size,
	    bool registered = false, std::uint32_t unitBytes = 1U);
	bool _bound{};
	bool _active{};
	bool _quarantined{};
	bool _sourceBorrowed{};

#if defined(LEXXOS_RK3506_DMA_SAFE)
	// NC staging plus a separate PL330 code page; retain both until reboot.
	std::uint8_t* _dmaStorage{};
	std::uint8_t* _stage{};
	std::uint8_t* _code{};
#else
	alignas(64) std::uint8_t _stage[4096]{};
	alignas(64) std::uint8_t _code[drivers::pl330::txProgramCapacity]{};
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE) && defined(LEXXOS_RK3506_LCD_DMA_NOTIFY_ISR)
	// Created from lcd-present at probe; lifetime extends until reboot.
	// Frame publication notifications use a different FreeRTOS primitive.
	SemaphoreHandle_t _completionSemaphore{};
#endif

	std::uint32_t volatile _irqDone{};
	std::uint64_t volatile _irqCycles{};
	std::uint32_t volatile _irqFault{};
	void (*_wait)(std::uint32_t){};
	std::uint8_t const* _areaSource{};
	std::size_t _areaCapacity{};
	bool _probed{};
	bool _enabled{};
	bool _failed{};
	char const* _reason{"not probed"};

	LcdDmaStats _stats{};
	LcdDmaFailure _failure{};
	std::uint32_t _codeBytes{};
};

} // namespace lexxos::platform::rk3506
