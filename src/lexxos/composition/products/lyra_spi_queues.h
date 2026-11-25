#pragma once
#include <lexxos/devices/bus/bus_transaction_queue.h>

namespace lexxos::composition::products {

devices::BusTransactionQueue& lyraSpi0Queue();
devices::BusTransactionQueue& lyraSpi1Queue();

} // namespace lexxos::composition::products
