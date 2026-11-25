#include <cstdlib>
#include <map>
#include <vector>

#include <lexxos/platform/rockchip/rk3506/dmac0_controller.h>

namespace {

void check(bool value) {
	if (!value) {
		std::abort();
	}
}

constexpr std::uintptr_t base = 0xFF000000U, route = 0xFF2880A8U, clockGate = 0xFF9A0814U;
std::map<std::uintptr_t, std::uint32_t> regs;
std::vector<unsigned> killed;
unsigned writes{}, enters{}, leaves{};
bool masked{}, rejectRoute{}, stuck{}, debugBusy{};
std::uint64_t counter{};

} // namespace

extern "C" std::uint64_t rk3506_timer_counter() {
	counter += 24000;
	return counter;
}

extern "C" std::uintptr_t dmac0_test_enter() {
	auto const old = masked;
	masked = true;
	++enters;
	return old;
}

extern "C" void dmac0_test_leave(std::uintptr_t old) {
	check(masked);
	masked = old != 0;
	++leaves;
}

extern "C" std::uint32_t dmac0_test_read(std::uintptr_t address) {
	if (address == base + 0x020 || address == route) {
		check(masked); // INTEN RMW and route check/write are CPU0 critical.
	}

	if (address == base + 0xD00) {
		return debugBusy ? 1 : 0;
	}

	return regs[address];
}

extern "C" void dmac0_test_write(std::uintptr_t address, std::uint32_t value) {
	++writes;
	if (address == base + 0x020) {
		check(masked);
		regs[address] = value;
	} else if (address == base + 0x02C) {
		check(value != 0 && (value & ~7U) == 0);
		regs[base + 0x028] &= ~value;
	} else if (address == route) {
		check(masked);
		if (!rejectRoute) {
			auto const mask = value >> 16U;
			regs[address] = (regs[address] & ~mask) | (value & mask);
		}
	} else if (address == clockGate) {
		check(value == (1U << 24U)); // Enable-only, never gate off or reset DMAC0.
	} else if (address == base + 0xD08 || address == base + 0xD0C) {
		check(!masked);
		regs[address] = value;
	} else if (address == base + 0xD04) {
		check(!masked && value == 0);
		auto const command = regs[base + 0xD08];
		check(((command >> 16U) & 255U) == 1U);
		auto const channel = (command >> 8U) & 7U;
		killed.push_back(channel);
		if (!stuck) {
			regs[base + 0x100 + channel * 8] = 0;
		}
	} else {
		check(false); // No SPI, reset, manager-kill or unrelated registers.
	}
}

int main() {
	using lexxos::platform::rk3506::dmac0Abort;
	using lexxos::platform::rk3506::dmac0Acknowledge;
	using lexxos::platform::rk3506::Dmac0Client;
	using lexxos::platform::rk3506::dmac0ConfigureSpiRoute;
	using lexxos::platform::rk3506::dmac0EnableClock;
	using lexxos::platform::rk3506::Dmac0RouteResult;
	using lexxos::platform::rk3506::dmac0SetEventsEnabled;
	constexpr auto sd = Dmac0Client::Sd, lcd = Dmac0Client::Lcd;
	dmac0EnableClock();
	regs[route] = 0xA500;
	check(dmac0ConfigureSpiRoute(lcd) == Dmac0RouteResult::Ready);
	check(dmac0ConfigureSpiRoute(sd) == Dmac0RouteResult::Ready && regs[route] == 0xA551);
	auto before = writes;
	check(dmac0ConfigureSpiRoute(sd) == Dmac0RouteResult::Ready && writes == before);
	regs[route] = 0xA5A1;
	check(dmac0ConfigureSpiRoute(sd) == Dmac0RouteResult::Conflict && writes == before);
	regs[route] = 0xA552;
	check(dmac0ConfigureSpiRoute(lcd) == Dmac0RouteResult::Conflict && writes == before);
	rejectRoute = true;
	regs[route] = 0xA550;
	check(dmac0ConfigureSpiRoute(lcd) == Dmac0RouteResult::WriteFailed && regs[route] == 0xA550);
	rejectRoute = false;

	regs[base + 0x020] = 0x80;
	dmac0SetEventsEnabled(sd, true);
	dmac0SetEventsEnabled(lcd, true);
	check(regs[base + 0x020] == 0x87);
	dmac0SetEventsEnabled(sd, false);
	check(regs[base + 0x020] == 0x84);
	masked = true;
	dmac0SetEventsEnabled(lcd, false);
	check(masked && regs[base + 0x020] == 0x80);
	regs[base + 0x028] = 0x87;
	dmac0Acknowledge(sd, 5); // Even an overbroad IRQ snapshot cannot clear LCD.
	check(regs[base + 0x028] == 0x86 && masked);
	dmac0Acknowledge(lcd);
	check(regs[base + 0x028] == 0x82);
	masked = false;

	regs[base + 0x020] = regs[base + 0x028] = 0x87;
	regs[base + 0x100] = regs[base + 0x108] = regs[base + 0x110] = 1;
	regs[base + 0x034] = 3; // SD faults cannot make LCD recovery fail.
	check(dmac0Abort(lcd));
	check(killed == std::vector<unsigned>{2});
	check(regs[base + 0x100] == 1 && regs[base + 0x108] == 1);
	check(regs[base + 0x020] == 0x83 && regs[base + 0x028] == 0x83);
	regs[base + 0x034] = 0;
	regs[base + 0x020] = regs[base + 0x028] = 0x87;
	check(dmac0Abort(sd));
	check(killed == std::vector<unsigned>{2, 0, 1});
	check(regs[base + 0x020] == 0x84 && regs[base + 0x028] == 0x84);

	regs[base + 0x030] = 1;
	check(!dmac0Abort(lcd)); // Shared manager fault is never reported recovered.
	regs[base + 0x030] = 0;
	regs[base + 0x034] = 4;
	check(!dmac0Abort(lcd));
	regs[base + 0x034] = 0;
	stuck = true;
	regs[base + 0x110] = 1;
	counter = (std::uint64_t{UINT32_MAX} - 9) * 24000;
	auto const started = counter;
	check(!dmac0Abort(lcd) && counter - started <= 22 * 24000);
	stuck = false;
	check(dmac0Abort(lcd));
	debugBusy = true;
	check(!dmac0Abort(lcd));
	debugBusy = false;
	check(dmac0Abort(lcd));

	auto invalid = static_cast<Dmac0Client>(99);
	before = writes;
	check(!dmac0Abort(invalid));
	check(dmac0ConfigureSpiRoute(invalid) == Dmac0RouteResult::InvalidClient);
	dmac0SetEventsEnabled(invalid, true);
	dmac0Acknowledge(invalid);
	dmac0Acknowledge(lcd, 3);
	check(writes == before && enters == leaves && !masked);
}
