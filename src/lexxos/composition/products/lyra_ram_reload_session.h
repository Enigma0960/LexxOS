#pragma once

#include <optional>

#include <lexxos/devices/bus/bus_transaction.h>

namespace lexxos::composition::products {

// Удерживает обе SPI до остановки CPU; отказ освобождает только своё владение.
class LyraRamReloadSession {
public:
	LyraRamReloadSession(devices::BusTransactionQueue& spi0, devices::BusTransactionQueue& spi1);
	bool prepare();

private:
	std::optional<devices::BusTransaction> _spi0;
	std::optional<devices::BusTransaction> _spi1;
	bool _prepared{false};
};

} // namespace lexxos::composition::products
