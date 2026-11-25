#include <cstdlib>
#include <iostream>

#include <lexxos/composition/products/lyra_ram_reload_session.h>
#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>

namespace {

lexxos::devices::BusTransactionQueue spi0, spi1;
bool lcdOk = true, sdOk = true;
unsigned lcdCalls = 0, sdCalls = 0;

void check(bool value) {
	if (!value) {
		std::cerr << "RAM reload contract failed\n";
		std::abort();
	}
}

void released() {
	check(!spi0.busy() && !spi1.busy());
}

} // namespace

namespace lexxos::platform::rk3506 {

bool lcdDmaPrepareRamReload() {
	check(spi0.busy() && spi1.busy());
	++lcdCalls;
	return lcdOk;
}

bool picoSdDmaPrepareRamReload() {
	check(spi0.busy() && spi1.busy());
	++sdCalls;
	return sdOk;
}

} // namespace lexxos::platform::rk3506

int main() {
	using lexxos::composition::products::LyraRamReloadSession;
	using lexxos::devices::BusTransaction;
	{
		BusTransaction owner{spi0};
		LyraRamReloadSession session{spi0, spi1};
		check(!session.prepare() && spi0.busy() && !spi1.busy());
	}
	released();
	{
		BusTransaction owner{spi1};
		LyraRamReloadSession session{spi0, spi1};
		check(!session.prepare() && !spi0.busy() && spi1.busy());
	}
	released();
	check(lcdCalls == 0 && sdCalls == 0);
	{
		LyraRamReloadSession session{spi0, spi0};
		check(!session.prepare());
	}
	released();
	{
		lcdOk = false;
		LyraRamReloadSession session{spi0, spi1};
		check(!session.prepare() && lcdCalls == 1 && sdCalls == 0);
	}
	released();
	{
		lcdOk = true;
		sdOk = false;
		LyraRamReloadSession session{spi0, spi1};
		check(!session.prepare() && lcdCalls == 2 && sdCalls == 1);
	}
	released();
	{
		sdOk = true;
		LyraRamReloadSession session{spi0, spi1};
		check(session.prepare() && lcdCalls == 3 && sdCalls == 2);
		check(session.prepare() && lcdCalls == 3 && sdCalls == 2);
		BusTransaction lcd{spi0}, sd{spi1};
		check(!lcd && !sd);
		// Возврат вместо park моделирует отказ следующего шага подготовки CPU.
	}
	released();
}
