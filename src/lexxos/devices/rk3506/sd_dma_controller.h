#pragma once

#include <lexxos/devices/rk3506/picosd_dma_pl330.h>

namespace lexxos::platform::rk3506 {

// One boot-lifetime owner of SPI1 DMA channels/events and persistent buffers.
// Callers serialize through SPI1 queue. IRQ binding cannot be replaced.
class SdDmaController {
public:
	SdDmaController();
	SdDmaController(SdDmaController const&) = delete;
	SdDmaController& operator=(SdDmaController const&) = delete;
	bool handleIrq(std::uint32_t id);
	void reset();
	bool probe();
	bool enabled();
	bool setEnabled(bool enabled);
	bool prepareRamReload();
	bool setDirect(bool enabled);
	bool direct();
	bool setWaitIrq(bool enabled);
	bool waitIrq();
	bool setWaitEvent(bool enabled);
	bool waitEvent();
	PicoSpi1DmaInfo info();
	PicoSdDmaStats statistics();
	PicoSdDmaFailure failure();
	PicoSdDmaMissedIrq missedIrq();
	PicoSdDmaIrqRegisters irqRegisters();
	bool readData(std::uint8_t* dest, std::size_t size);

private:
	friend bool sdDmaBindController(SdDmaController&);
	void stopEventWait();
	void captureFailure(char const* stage);

	void buildCode();
	void kill();
#if defined(LEXXOS_RK3506_DMA_SAFE)
	bool prepareDmaBuffers();
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE)
	// One boot-lifetime allocation; keep all microcode and bounce buffers
	// in the Normal-NC .dma pool. Never release while PL330 may be running.
	std::uint8_t* _dmaStorage{};
	std::uint8_t* _tx{};
	std::uint8_t* _rx{};
	std::uint8_t* _rxCode{};
	std::uint8_t* _txCode{};
#else
	alignas(64) std::uint8_t _tx[512];
	alignas(64) std::uint8_t _rx[512];
	alignas(64) std::uint8_t _rxCode[64]{};
	alignas(64) std::uint8_t _txCode[64]{};
#endif

	bool _bound{};
	bool _probed{};
	bool _enabled{};
	bool _fault{};
	bool _directTarget{}; // Opt-in only; buffered path remains default.
	bool _waitIrq{}; // Opt-in only. Default DMA completion remains polling.
	bool _waitEvent{}; // Timer event stream: no DMAC0 GIC IRQ required.
	std::uint32_t _savedEventBits{};
	std::uint32_t volatile _irqEvents{};
	std::uint32_t volatile _irqFault{};
	std::uint32_t _rxDarOffset{}; // Offset of DMAMOV DAR immediate in RX microcode.
	std::uint32_t _rxCurrentDestination{};
	char const* _reason{"run picosd dma probe"};
	PicoSdDmaStats _stats{};
	PicoSdDmaFailure _failure{};
	PicoSdDmaMissedIrq _missedIrq{};
};

bool sdDmaBindController(SdDmaController& controller);

} // namespace lexxos::platform::rk3506
