#include <lexxos/composition/products/lyra_ram_reload_session.h>

#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>

namespace lexxos::composition::products {

LyraRamReloadSession::LyraRamReloadSession(devices::BusTransactionQueue& spi0, devices::BusTransactionQueue& spi1) {
	// Не ждём вторую шину, удерживая первую: активный caller может владеть обеими.
	_spi0.emplace(spi0, devices::DeviceResourcePriority::System, 0);
	if (!bool(*_spi0)) {
		_spi0.reset();
		return;
	}

	_spi1.emplace(spi1, devices::DeviceResourcePriority::System, 0);
	if (!bool(*_spi1)) {
		_spi1.reset();
		_spi0.reset();
	}
}

bool LyraRamReloadSession::prepare() {
	if (!_spi0 || !_spi1) {
		return false;
	}

	if (_prepared) {
		return true;
	}

	// При частичном отказе DMA может остаться выключенной; PIO остаётся fallback.
	if (!platform::rk3506::lcdDmaPrepareRamReload() || !platform::rk3506::picoSdDmaPrepareRamReload()) {
		return false;
	}

	_prepared = true;
	return true;
}

} // namespace lexxos::composition::products
