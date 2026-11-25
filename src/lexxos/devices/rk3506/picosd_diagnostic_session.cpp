#include <lexxos/devices/rk3506/picosd_diagnostic_session.h>

#include <lexxos/devices/rk3506/native_picosd_transport.h>

namespace lexxos::devices::rk3506 {

PicoSdDiagnosticSession::PicoSdDiagnosticSession(BusTransactionQueue& queue, NativePicoSdTransport& transport)
    : _transport(transport)
    , _transaction(queue, DeviceResourcePriority::Interactive, 100)
    , _active(transport.prepareDiagnostics(_transaction)) {
}

bool PicoSdDiagnosticSession::read(std::uint64_t lba, std::uint8_t* data, std::size_t count) {
	return _active && _transport.readForDiagnostics(_transaction, lba, data, count);
}

} // namespace lexxos::devices::rk3506
