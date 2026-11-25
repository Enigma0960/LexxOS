#include <lexxos/platform/rockchip/rk3506/dmac0_controller.h>

#include <lexxos/drivers/core/dma/pl330_debug_port.h>
#include <lexxos/platform/rockchip/rk3506/dma_resources.h>

#if defined(LEXXOS_RK3506_DMA_SAFE)
	#include <lexxos/platform/rockchip/rk3506/mmu_managed.h>
#endif

extern "C" std::uint64_t rk3506_timer_counter();
#if defined(LEXXOS_RK3506_DMAC0_TEST)
extern "C" std::uint32_t dmac0_test_read(std::uintptr_t);
extern "C" void dmac0_test_write(std::uintptr_t, std::uint32_t);
extern "C" std::uintptr_t dmac0_test_enter();
extern "C" void dmac0_test_leave(std::uintptr_t);
#endif

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uintptr_t base = 0xFF000000U;

	std::uint32_t readAddress(std::uintptr_t address) {
#if defined(LEXXOS_RK3506_DMAC0_TEST)
		return dmac0_test_read(address);
#else
		return *reinterpret_cast<std::uint32_t const volatile*>(address);
#endif
	}

	void writeAddress(std::uintptr_t address, std::uint32_t value) {
#if defined(LEXXOS_RK3506_DMAC0_TEST)
		dmac0_test_write(address, value);
#else
		*reinterpret_cast<std::uint32_t volatile*>(address) = value;
#endif
	}

	std::uint32_t read(void*, std::uint32_t offset) {
		return readAddress(base + offset);
	}

	void write(void*, std::uint32_t offset, std::uint32_t value) {
		writeAddress(base + offset, value);
	}

	void barrier(void*) {
#if !defined(LEXXOS_RK3506_DMAC0_TEST)
		asm volatile("dsb sy" ::: "memory");
#endif
	}

	std::uint32_t nowMs(void*) {
		// RK3506 counter runs at 24 MHz even before timer IRQ startup.
		return static_cast<std::uint32_t>(rk3506_timer_counter() / 24000U);
	}

	std::uintptr_t enter(void*) {
#if defined(LEXXOS_RK3506_DMAC0_TEST)
		return dmac0_test_enter();
#else
		std::uint32_t state;
		asm volatile("mrs %0, cpsr; cpsid i" : "=r"(state) : : "memory");
		return state;
#endif
	}

	void leave(void*, std::uintptr_t state) {
#if defined(LEXXOS_RK3506_DMAC0_TEST)
		dmac0_test_leave(state);
#else
		if ((state & 0x80U) == 0U) {
			asm volatile("cpsie i" ::: "memory");
		}
#endif
	}

	std::uint32_t channels(Dmac0Client client) {
		switch (client) {
			case Dmac0Client::Sd : return dma::sdChannels;
			case Dmac0Client::Lcd: return dma::lcdChannels;
		}
		return 0;
	}

	std::uint32_t events(Dmac0Client client) {
		switch (client) {
			case Dmac0Client::Sd : return dma::sdEvents;
			case Dmac0Client::Lcd: return dma::lcdEvents;
		}
		return 0;
	}

	drivers::pl330::Pl330DebugPort port{
	    {nullptr, read, write, barrier, nowMs, enter, leave}
    };

} // namespace

void dmac0EnableClock() {
	writeAddress(0xFF9A0814U, 1U << 24U); // ACLK_DMAC0: hiword mask bit 8, enable only.
	barrier(nullptr);
}

Dmac0RouteResult dmac0ConfigureSpiRoute(Dmac0Client client) {
	std::uint32_t mask{}, route{};
	switch (client) {
		case Dmac0Client::Sd:
			mask = 0xF0U;
			route = 0x50U;
			break;
		case Dmac0Client::Lcd:
			mask = 0x03U;
			route = 0x01U;
			break;
		default: return Dmac0RouteResult::InvalidClient;
	}

	constexpr std::uintptr_t address = 0xFF2880A8U;
	auto const state = enter(nullptr);
	auto const old = readAddress(address) & mask;
	Dmac0RouteResult result = Dmac0RouteResult::Ready;
	if (old != 0 && old != route) {
		result = Dmac0RouteResult::Conflict;
	} else {
		if (old != route) {
			writeAddress(address, (mask << 16U) | route);
			barrier(nullptr);
		}

		if ((readAddress(address) & mask) != route) {
			result = Dmac0RouteResult::WriteFailed;
		}
	}

	leave(nullptr, state);
	return result;
}

void dmac0SetEventsEnabled(Dmac0Client client, bool enabled) {
	auto const mask = events(client);
	if (!mask) {
		return;
	}

	auto const state = enter(nullptr);
	auto const current = read(nullptr, 0x020U);
	write(nullptr, 0x020U, enabled ? current | mask : current & ~mask);
	barrier(nullptr);
	leave(nullptr, state);
}

void dmac0Acknowledge(Dmac0Client client, std::uint32_t pending) {
	auto const mask = events(client) & pending;
	if (mask) {
		write(nullptr, 0x02CU, mask);
		barrier(nullptr);
	}
}

bool dmac0Abort(Dmac0Client client) {
	auto const owned = channels(client);
	if (!owned) {
		return false;
	}

	dmac0SetEventsEnabled(client, false);
	bool submitted = true;
	for (unsigned channel = 0; channel < 8; ++channel) {
		if (owned & (1U << channel)) {
			submitted = port.kill(owned, channel) && submitted;
		}
	}

	auto const start = nowMs(nullptr);
	bool idle{};
	do {
		idle = true;
		for (unsigned channel = 0; channel < 8; ++channel) {
			if ((owned & (1U << channel)) && (read(nullptr, 0x100U + channel * 8U) & 15U)) {
				idle = false;
			}
		}
	} while (!idle && std::uint32_t(nowMs(nullptr) - start) < 20U);

	dmac0Acknowledge(client);
	return submitted && idle && !dma::hasFault(read(nullptr, 0x030U), read(nullptr, 0x034U), owned);
}

bool dmac0StartNonsecure(Dmac0Client client, unsigned channel, std::uint32_t programAddress) {
#if defined(LEXXOS_RK3506_DMA_SAFE)
	// Microcode must reside in the non-cacheable pool: the PL330 fetches
	// instructions from memory, not through the CPU D-cache. This guard is
	// shared by the SD and LCD owners and cannot be bypassed by a caller.
	auto const* program = reinterpret_cast<void const*>(static_cast<std::uintptr_t>(programAddress));
	if (!memory::snapshot().active || !memory::isDmaBuffer(program, 1U)) {
		return false;
	}

	return port.startNonsecure(channels(client), channel, programAddress);
#elif defined(LEXXOS_RK3506_FREERTOS) || defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
	(void) client;
	(void) channel;
	(void) programAddress;
	return false;
#else
	return port.startNonsecure(channels(client), channel, programAddress);
#endif
}

bool dmac0Kill(Dmac0Client client, unsigned channel) {
	return port.kill(channels(client), channel);
}

} // namespace lexxos::platform::rk3506
