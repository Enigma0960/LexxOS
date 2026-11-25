#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <lexxos/composition/products/lyra_sd_dma.h>
#include <lexxos/composition/products/lyra_sd_protocol.h>
#include <lexxos/devices/rk3506/sd_dma_controller.h>
#include <lexxos/devices/rk3506/sd_protocol.h>
#include <lexxos/devices/rk3506/sd_spi_controller.h>
#include <lexxos/platform/rockchip/rk3506/sd_gpio.h>

#include "fake_sd_protocol_io.h"

namespace {

unsigned hardwareCalls{}, resets{}, gpioBytes{};
bool inserted{true};
std::uint32_t ticks{};

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

struct Media {
	bool mounted{};
	unsigned suppressions{};
};

lexxos::platform::rk3506::PicoSdMediaHooks hooks(Media& media) {
	return {&media, [](void* p) { return static_cast<Media*>(p)->mounted; },
	    [](void* p) { ++static_cast<Media*>(p)->suppressions; }};
}

} // namespace

extern "C" std::uint32_t rk3506_timer_ticks() {
	return ticks += 100;
}

extern "C" std::uint64_t rk3506_timer_counter() {
	return ++ticks;
}

extern "C" std::uint32_t rk3506_timer_counter_frequency() {
	return 24000000U;
}

namespace lexxos::platform::rk3506 {

SdDmaController::SdDmaController() = default;

void sdGpioConfigure() {
	++hardwareCalls;
}

void sdGpioChipSelect(bool) {
	++hardwareCalls;
}

std::uint8_t sdGpioTransfer(std::uint8_t) {
	++hardwareCalls;
	++gpioBytes;
	return 0xFF;
}

void sdGpioEnableClock() {
	++hardwareCalls;
}

bool sdGpioCardInserted() {
	++hardwareCalls;
	return inserted;
}

SdGpioRegisters sdGpioRegisters(bool configured) {
	return {0, 0, 0, 0, configured, !inserted};
}

bool SdSpiController::initialize() {
	++hardwareCalls;
	return false;
}

void SdSpiController::stop() {
	++hardwareCalls;
}

bool SdSpiController::transfer(std::uint8_t, std::uint8_t&) {
	std::abort();
}

bool SdSpiController::readData(std::uint8_t*, std::size_t) {
	std::abort();
}

void SdSpiController::fastClock() {
	std::abort();
}

bool SdSpiController::setClockMHz(std::uint32_t) {
	std::abort();
}

void SdDmaController::reset() {
	++resets;
}

bool SdDmaController::enabled() {
	return false;
}

bool SdDmaController::probe() {
	std::abort();
}

bool SdDmaController::readData(std::uint8_t*, std::size_t) {
	std::abort();
}

bool SdDmaController::setDirect(bool) {
	std::abort();
}

bool SdDmaController::setEnabled(bool) {
	std::abort();
}

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

platform::rk3506::SdDmaController& lyraSdDma() {
	static platform::rk3506::SdDmaController controller;
	return controller;
}

} // namespace lexxos::composition::products

int main() {
	namespace sd = lexxos::platform::rk3506;
	auto& primary = lexxos::composition::products::lyraSdProtocol();
	FakeSdProtocolIo secondIo;
	sd::SdProtocol second{secondIo};
	expect(hardwareCalls == 0 && resets == 0 && secondIo.calls == 0, "constructor touched hardware");
	expect(!sd::picoSdProbeManual() && !second.selectTransport(true) && hardwareCalls == 0, "missing hooks allowed probe/configuration");
	Media firstMedia{}, secondMedia{true};
	sd::picoSdSetMediaHooks(hooks(firstMedia));
	second.setMediaHooks(hooks(secondMedia));
	expect(sd::picoSdSelectTransport(true), "unmounted transport selection rejected");
	expect(primary.diagnosticStatus().requestSpi1 && !second.diagnosticStatus().requestSpi1, "transport state leaked");
	expect(firstMedia.suppressions == 1 && secondMedia.suppressions == 0, "wrong hook context");
	expect(!second.selectTransport(true) && !second.probeManual() && hardwareCalls == 0, "mounted media touched hardware");
	// Native SPI init failure belongs only to the probed session.
	expect(!sd::picoSdProbeManual(), "failed SPI initialize accepted");
	auto failed = primary.diagnosticStatus();
	expect(failed.configured && failed.spi1Fault && !failed.ready && std::string_view{failed.step} == "SPI1-init", "failure status lost");
	expect(gpioBytes == 12 && resets == 1, "startup clocks/reset changed");
	expect(!second.diagnosticStatus().configured && !second.diagnosticStatus().spi1Fault, "failure leaked to other session");
	std::array<std::uint8_t, 512> data{};
	expect(!sd::picoSdReadSector(0, data.data()), "unready read accepted");
	expect(primary.profileStatistics().readErrors == 1 && second.profileStatistics().readErrors == 0, "read profile shared");
	expect(!primary.writeSectors(0, data.data(), 1) && !primary.readSectors(0, data.data(), 0), "unready I/O accepted");
	expect(!primary.setCrcMode(sd::PicoSdCrcMode::Bitwise) && primary.crcMode() == sd::PicoSdCrcMode::Slicing4, "unready CRC policy changed");
	sd::picoSdInvalidateRemoved();
	expect(!primary.diagnosticStatus().configured && primary.sectorCount() == 0 && std::string_view{primary.diagnosticStatus().step} == "removed", "removal state not reset");
	// Changing hooks is local, and callbacks observe the live context.
	auto const nativeCalls = hardwareCalls;
	auto const nativeResets = resets;
	secondMedia.mounted = false;
	expect(second.selectTransport(false) && secondMedia.suppressions == 1, "second hooks ignored live state");
	expect(!second.probeManual() && std::string_view{second.diagnosticStatus().step} == "CMD0", "GPIO probe failure not local");
	expect(std::string_view{primary.diagnosticStatus().step} == "removed" && firstMedia.suppressions == 1, "probe/hook state leaked");
	expect(secondIo.gpioBytes > 12 && secondIo.resets == 1 && secondIo.timerCalls > 0, "GPIO probe bypassed injected I/O/time");
	expect(hardwareCalls == nativeCalls && resets == nativeResets, "second session reached native backend");
	secondIo.inserted = false;
	expect(!second.cardInserted(), "detect used native card state");
	auto snapshot = second.registerStatus();
	expect(snapshot.gpioGate == 11 && snapshot.spiGate == 22 && snapshot.iocC == 33 && snapshot.iocD == 44 && snapshot.detectHigh, "register snapshot used wrong backend");
	secondIo.inserted = true;
	secondIo.spiInitOk = true;
	expect(second.selectTransport(true), "SPI fixture selection failed");
	auto const gpioBefore = secondIo.gpioBytes;
	expect(!second.probeManual() && second.diagnosticStatus().spi1Fault, "SPI transfer error was lost");
	expect(secondIo.spiBytes > 0 && secondIo.gpioBytes == gpioBefore + 12, "SPI failure fell back to GPIO");
	expect(hardwareCalls == nativeCalls && resets == nativeResets, "SPI probe reached native backend");
}
