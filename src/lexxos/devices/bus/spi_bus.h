#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/bus_transaction_queue.h>
#include <lexxos/devices/bus/spi_bus_transaction.h>
#include <lexxos/devices/bus/spi_clock_mode.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {

class SpiBus {
public:
	SpiBus() = default;

	// Для нескольких CS на одном контроллере composition передаёт одну очередь.
	// Очередь должна пережить все связанные с ней устройства.
	explicit SpiBus(BusTransactionQueue& queue);

	virtual ~SpiBus() = default;

	void setTransactionWaitFunction(BusTransactionQueue::WaitFunction wait);

	[[nodiscard]] bool transactionBusy() const;

	[[nodiscard]] std::uint32_t pendingTransactions() const;

	virtual bool initialize(lexxos::platform::SpiPins const& pins) = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
	virtual bool setBaudRate(std::uint32_t baudRate) = 0;
	virtual bool setClockMode(SpiClockMode mode) = 0;
	[[nodiscard]] virtual SpiClockMode clockMode() const = 0;
	virtual void select() = 0;
	virtual void deselect() = 0;
	virtual bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) = 0;
	virtual bool write(std::uint8_t const* data, std::size_t size) = 0;
	virtual bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) = 0;

	virtual bool beginTransaction();

	virtual void endTransaction();

	[[nodiscard]] virtual bool supportsDma() const;

	virtual bool startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size);

	virtual bool waitDmaTransfer(std::uint32_t timeoutMs);

	virtual bool transferDma(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size, std::uint32_t timeoutMs);

private:
	friend class SpiBusTransaction;
	BusTransactionQueue _ownedQueue{};
	BusTransactionQueue* _queue{&_ownedQueue};
};

SpiBus& sdCardSpiBus();
SpiBus& displaySpiBus();

} // namespace lexxos::devices
