#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/i2c_bus_transaction.h>
#include <lexxos/devices/bus/i2c_transaction_queue.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {

struct I2cFailureSnapshot {
	bool available{};
	bool nack{};
	std::uint32_t event{};
	std::uint32_t pending{};
	std::uint32_t recoveryAttempts{};
	std::uint32_t recoverySuccesses{};
};

class I2cBus {
public:
	virtual ~I2cBus() = default;

	virtual bool initialize(lexxos::platform::I2cPins const& pins) = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
	virtual bool probe(std::uint8_t address) = 0;
	[[nodiscard]] virtual I2cFailureSnapshot lastFailure() const;
	virtual bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) = 0;

	virtual bool read(std::uint8_t address, std::uint8_t* data, std::size_t size);

	// START + запись + repeated START + чтение + STOP. Порт без этой операции возвращает false.
	virtual bool writeRead(std::uint8_t, std::uint8_t const*, std::size_t, std::uint8_t*, std::size_t);

	virtual bool beginTransaction();

	virtual void endTransaction();

	void setTransactionWaitFunction(I2cTransactionQueue::WaitFunction wait);

	[[nodiscard]] bool transactionBusy() const;

	[[nodiscard]] std::uint32_t pendingTransactions() const;

private:
	friend class I2cBusTransaction;
	I2cTransactionQueue _queue{};
};

#if !defined(LEXXOS_EDITION_CE)
I2cBus& debugI2cBus();
I2cBus& systemI2cBus();
#endif

} // namespace lexxos::devices
