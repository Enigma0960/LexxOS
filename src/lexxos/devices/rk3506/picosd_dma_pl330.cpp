#include <lexxos/composition/products/lyra_sd_dma.h>
#include <lexxos/devices/rk3506/sd_dma_controller.h>

namespace lexxos::platform::rk3506 {

namespace {

	SdDmaController* boundController{};

} // namespace

bool sdDmaBindController(SdDmaController& controller) {
	if (boundController && boundController != &controller) {
		return false;
	}

	boundController = &controller;
	controller._bound = true;
	return true;
}

// No lazy composition initialization from interrupt context.
extern "C" bool rk3506_dma_irq_handle(std::uint32_t id) {
	return boundController && boundController->handleIrq(id);
}

void picoSdDmaReset() {
	composition::products::lyraSdDma().reset();
}

bool picoSdDmaProbe() {
	return composition::products::lyraSdDma().probe();
}

bool picoSdDmaEnabled() {
	return composition::products::lyraSdDma().enabled();
}

bool picoSdDmaSetEnabled(bool enabled) {
	return composition::products::lyraSdDma().setEnabled(enabled);
}

bool picoSdDmaPrepareRamReload() {
	return composition::products::lyraSdDma().prepareRamReload();
}

bool picoSdDmaSetDirect(bool enabled) {
	return composition::products::lyraSdDma().setDirect(enabled);
}

bool picoSdDmaDirect() {
	return composition::products::lyraSdDma().direct();
}

bool picoSdDmaSetWaitIrq(bool enabled) {
	return composition::products::lyraSdDma().setWaitIrq(enabled);
}

bool picoSdDmaWaitIrq() {
	return composition::products::lyraSdDma().waitIrq();
}

bool picoSdDmaSetWaitEvent(bool enabled) {
	return composition::products::lyraSdDma().setWaitEvent(enabled);
}

bool picoSdDmaWaitEvent() {
	return composition::products::lyraSdDma().waitEvent();
}

PicoSpi1DmaInfo picoSdDmaInfo() {
	return composition::products::lyraSdDma().info();
}

PicoSdDmaStats picoSdDmaStatistics() {
	return composition::products::lyraSdDma().statistics();
}

PicoSdDmaFailure picoSdDmaFailure() {
	return composition::products::lyraSdDma().failure();
}

PicoSdDmaMissedIrq picoSdDmaMissedIrq() {
	return composition::products::lyraSdDma().missedIrq();
}

PicoSdDmaIrqRegisters picoSdDmaIrqRegisters() {
	return composition::products::lyraSdDma().irqRegisters();
}

bool picoSdDmaReadData(std::uint8_t* dest, std::size_t size) {
	return composition::products::lyraSdDma().readData(dest, size);
}

} // namespace lexxos::platform::rk3506
