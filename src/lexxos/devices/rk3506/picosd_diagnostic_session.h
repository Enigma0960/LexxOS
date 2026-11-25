#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/bus/bus_transaction.h>

namespace lexxos::devices::rk3506 {

class NativePicoSdTransport;

// Составная maintenance-операция; настройки восстанавливаются до уничтожения сессии.
class PicoSdDiagnosticSession {
public:
	PicoSdDiagnosticSession(BusTransactionQueue& queue, NativePicoSdTransport& transport);

	[[nodiscard]] explicit operator bool() const { return _active; }

	bool read(std::uint64_t lba, std::uint8_t* data, std::size_t count);

private:
	NativePicoSdTransport& _transport;
	BusTransaction _transaction;
	bool _active;
};

} // namespace lexxos::devices::rk3506
