#pragma once

namespace lexxos::devices {

class BusTransactionQueue;

namespace detail {

	class BusTransactionQueueGuard {
	public:
		explicit BusTransactionQueueGuard(BusTransactionQueue const& queue);

		~BusTransactionQueueGuard();

		BusTransactionQueueGuard(BusTransactionQueueGuard const&) = delete;
		BusTransactionQueueGuard& operator=(BusTransactionQueueGuard const&) = delete;

	private:
		BusTransactionQueue const& _queue;
	};

} // namespace lexxos::devices::detail

} // namespace lexxos::devices
