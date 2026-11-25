#include <lexxos/devices/rk3506/lcd_dma_controller.h>

namespace lexxos::platform::rk3506 {

namespace {

	LcdDmaController* boundController{};

} // namespace

bool lcdDmaBindController(LcdDmaController& controller) {
	// Boot-only, before enabling LCD IRQ. Rebinding could abandon live DMA memory.
	if (boundController && boundController != &controller) {
		return false;
	}

	boundController = &controller;
	controller._bound = true;

	return true;
}

bool lcdDmaSourceRetained() {
	return boundController && boundController->sourceRetained();
}

bool lcdDmaQuarantined() {
	return boundController && boundController->quarantined();
}

extern "C" bool rk3506_lcd_dma_irq_handle(std::uint32_t id) {
	return boundController && boundController->handleIrq(id);
}

void lcdDmaSetWaitFunction(void (*wait)(std::uint32_t)) {
	if (boundController) {
		boundController->setWaitFunction(wait);
	}
}

bool lcdDmaProbe() {
	return boundController && boundController->probe();
}

bool lcdDmaSetEnabled(bool enabled) {
	return boundController && boundController->setEnabled(enabled);
}

bool lcdDmaEnabled() {
	return boundController && boundController->enabled();
}

bool lcdDmaPrepareRamReload() {
	return boundController && boundController->prepareRamReload();
}

bool lcdDmaRegisterSource(std::uint8_t const* source, std::size_t capacity) {
	return boundController && boundController->registerSource(source, capacity);
}

bool lcdDmaWrite(std::uint8_t const* source, std::size_t count) {
	return boundController && boundController->write(source, count);
}

bool lcdDmaWrite16(std::uint16_t const* source, std::size_t pixels) {
	return boundController && boundController->write16(source, pixels);
}

char const* lcdDmaReason() {
	return boundController ? boundController->reason() : "LCD DMA owner not bound";
}

LcdDmaStats lcdDmaStatistics() {
	return boundController ? boundController->statistics() : LcdDmaStats{};
}

LcdDmaFailure lcdDmaLastFailure() {
	return boundController ? boundController->lastFailure() : LcdDmaFailure{};
}

LcdDmaRegisters lcdDmaRegisterSnapshot() {
	return boundController ? boundController->registerSnapshot() : LcdDmaRegisters{};
}

} // namespace lexxos::platform::rk3506
