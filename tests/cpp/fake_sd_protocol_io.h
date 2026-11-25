#pragma once

#include <cstdlib>

#include <lexxos/devices/rk3506/sd_protocol_io.h>

// No native symbols: a second session must exclusively use its own backend.
class FakeSdProtocolIo final : public lexxos::platform::rk3506::SdProtocolIo {
public:
	unsigned calls{}, resets{}, gpioBytes{}, spiBytes{}, timerCalls{};
	bool inserted{true}, spiInitOk{};
	std::uint32_t now{};

	void gpioConfigure() override { ++calls; }

	void gpioChipSelect(bool) override { ++calls; }

	std::uint8_t gpioTransfer(std::uint8_t) override {
		++calls;
		++gpioBytes;
		return 0xFF;
	}

	void gpioEnableClock() override { ++calls; }

	bool gpioCardInserted() override {
		++calls;
		return inserted;
	}

	lexxos::platform::rk3506::PicoSdRegisterStatus gpioRegisters(bool configured) override {
		++calls;
		return {11, 22, 33, 44, configured, !inserted};
	}

	bool spiInitialize() override {
		++calls;
		return spiInitOk;
	}

	void spiStop() override { ++calls; }

	bool spiTransfer(std::uint8_t, std::uint8_t&) override {
		++calls;
		++spiBytes;
		return false;
	}

	bool spiReadData(std::uint8_t*, std::size_t) override { std::abort(); }

	void spiFastClock() override { std::abort(); }

	bool spiSetClockMHz(std::uint32_t) override { std::abort(); }

	void dmaReset() override {
		++calls;
		++resets;
	}

	bool dmaEnabled() override {
		++calls;
		return false;
	}

	bool dmaProbe() override { std::abort(); }

	bool dmaReadData(std::uint8_t*, std::size_t) override { std::abort(); }

	bool dmaSetDirect(bool) override { std::abort(); }

	bool dmaSetEnabled(bool) override { std::abort(); }

	std::uint32_t ticks() override {
		++timerCalls;
		return now += 100;
	}

	std::uint64_t counter() override {
		++timerCalls;
		return ++now;
	}
};
