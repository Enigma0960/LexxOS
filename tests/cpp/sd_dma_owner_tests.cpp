#include <cstdlib>
#include <iostream>
#include <type_traits>

#include <lexxos/composition/products/lyra_sd_dma.h>
#include <lexxos/devices/rk3506/sd_dma_controller.h>

namespace {

unsigned constructions{}, calls{};
lexxos::platform::rk3506::SdDmaController* receiver{};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

namespace lexxos::platform::rk3506 {

extern "C" bool rk3506_dma_irq_handle(std::uint32_t id);

SdDmaController::SdDmaController() {
	++constructions;
}

bool SdDmaController::handleIrq(std::uint32_t id) {
	receiver = this;
	++calls;
	(void) id;
	return true;
}

void SdDmaController::reset() {
	receiver = this;
	++calls;
}

bool SdDmaController::probe() {
	receiver = this;
	++calls;
	return true;
}

bool SdDmaController::enabled() {
	receiver = this;
	++calls;
	return true;
}

bool SdDmaController::setEnabled(bool enabled) {
	receiver = this;
	++calls;
	(void) enabled;
	return true;
}

bool SdDmaController::prepareRamReload() {
	receiver = this;
	++calls;
	return true;
}

bool SdDmaController::setDirect(bool enabled) {
	receiver = this;
	++calls;
	(void) enabled;
	return true;
}

bool SdDmaController::direct() {
	receiver = this;
	++calls;
	return true;
}

bool SdDmaController::setWaitIrq(bool enabled) {
	receiver = this;
	++calls;
	(void) enabled;
	return true;
}

bool SdDmaController::waitIrq() {
	receiver = this;
	++calls;
	return true;
}

bool SdDmaController::setWaitEvent(bool enabled) {
	receiver = this;
	++calls;
	(void) enabled;
	return true;
}

bool SdDmaController::waitEvent() {
	receiver = this;
	++calls;
	return true;
}

PicoSpi1DmaInfo SdDmaController::info() {
	receiver = this;
	++calls;
	return {};
}

PicoSdDmaStats SdDmaController::statistics() {
	receiver = this;
	++calls;
	return {};
}

PicoSdDmaFailure SdDmaController::failure() {
	receiver = this;
	++calls;
	return {};
}

PicoSdDmaMissedIrq SdDmaController::missedIrq() {
	receiver = this;
	++calls;
	return {};
}

PicoSdDmaIrqRegisters SdDmaController::irqRegisters() {
	receiver = this;
	++calls;
	return {};
}

bool SdDmaController::readData(std::uint8_t* dest, std::size_t size) {
	receiver = this;
	++calls;
	(void) dest;
	(void) size;
	return true;
}

} // namespace lexxos::platform::rk3506

int main() {
	namespace sd = lexxos::platform::rk3506;
	static_assert(!std::is_copy_constructible_v<sd::SdDmaController>);
	expect(!sd::rk3506_dma_irq_handle(148) && constructions == 0, "IRQ initialized an owner before binding");
	auto& owner = lexxos::composition::products::lyraSdDma();
	expect(constructions == 1 && calls == 0, "composition touched hardware");
	expect(&owner == &lexxos::composition::products::lyraSdDma() && constructions == 1, "duplicate owner");
	expect(sd::sdDmaBindController(owner), "same binding was refused");
	sd::SdDmaController other;
	expect(!sd::sdDmaBindController(other), "IRQ binding was replaced");
	expect(sd::rk3506_dma_irq_handle(148) && receiver == &owner, "IRQ used wrong instance");
	sd::picoSdDmaReset();
	expect(receiver == &owner, "legacy reset used wrong instance");
	(void) sd::picoSdDmaStatistics();
	expect(receiver == &owner, "legacy statistics used wrong instance");
	expect(sd::picoSdDmaSetEnabled(true) && receiver == &owner, "legacy mode used wrong instance");
}
