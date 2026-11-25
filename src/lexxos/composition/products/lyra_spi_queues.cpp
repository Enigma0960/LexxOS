#include <lexxos/composition/products/lyra_spi_queues.h>

namespace lexxos::composition::products {

devices::BusTransactionQueue& lyraSpi0Queue() {
	static devices::BusTransactionQueue queue;
	return queue;
}

devices::BusTransactionQueue& lyraSpi1Queue() {
	static devices::BusTransactionQueue queue;
	return queue;
}

} // namespace lexxos::composition::products
