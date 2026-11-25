#include <lexxos/platform/rockchip/rk3506/dma_irq_clients.h>

namespace lexxos::platform::rk3506 {

DmaIrqClients::DmaIrqClients(Configure configure)
    : _configure(configure) {
}

bool DmaIrqClients::setEnabled(unsigned client, bool enabled) {
	if (client >= 2 || !_configure) {
		return false;
	}

	auto const bit = 1U << client;
	auto const next = enabled ? _clients | bit : _clients & ~bit;
	if (next == _clients) {
		return true;
	}

	if ((_clients == 0 || next == 0) && !_configure(next != 0)) {
		return false;
	}

	_clients = next;
	return true;
}

} // namespace lexxos::platform::rk3506
