#include <lexxos/devices/rk3506/native_picosd_transport.h>

#include <lexxos/devices/bus/bus_transaction.h>
#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
#include <lexxos/devices/rk3506/picosd_spi1_hw.h>
#include <lexxos/devices/rk3506/sd_protocol.h>

namespace lexxos::devices::rk3506 {

NativePicoSdTransport::NativePicoSdTransport(BusTransactionQueue& queue, platform::rk3506::SdProtocol& protocol)
    : _queue(queue)
    , _protocol(protocol) {
}

void NativePicoSdTransport::finishRemoval() {
	if (_removalPending) {
		_protocol.invalidateRemoved();
		_removalPending = false;
	}
}

bool NativePicoSdTransport::prepareDiagnostics(BusTransaction const& transaction) {
	if (!transaction.owns(_queue)) {
		return false;
	}

	finishRemoval();
	return true;
}

bool NativePicoSdTransport::readForDiagnostics(BusTransaction const& transaction,
    std::uint64_t lba, std::uint8_t* data, std::size_t count) {
	if (!transaction.owns(_queue) || !ready() || data == nullptr || count == 0 || lba > UINT32_MAX) {
		return false;
	}

	auto const sectors = sectorCount();
	if (lba >= sectors || count > sectors - lba || count - 1 > UINT32_MAX - lba) {
		return false;
	}

	return _protocol.readSectors(static_cast<std::uint32_t>(lba), data, count);
}

bool NativePicoSdTransport::ready() {
	return !_removalPending && _protocol.ready();
}

bool NativePicoSdTransport::inserted() {
	return _protocol.cardInserted();
}

char const* NativePicoSdTransport::failureStage() {
	return _protocol.diagnosticStatus().step;
}

std::uint8_t NativePicoSdTransport::lastResponse() {
	return _protocol.diagnosticStatus().response;
}

bool NativePicoSdTransport::probe() {
	BusTransaction transaction{_queue};
	if (!transaction) {
		return false;
	}

	finishRemoval();
	return _protocol.probeAuto();
}

void NativePicoSdTransport::invalidateRemoved() {
	// Немедленно закрываем доступ к носителю, но не сбрасываем чужую транзакцию.
	_removalPending = true;
	BusTransaction transaction{_queue};
	if (transaction) {
		finishRemoval();
	}
}

std::uint64_t NativePicoSdTransport::sectorCount() {
	return _removalPending ? 0 : _protocol.sectorCount();
}

bool NativePicoSdTransport::read(std::uint32_t lba, std::uint8_t* data, std::size_t count) {
	BusTransaction transaction{_queue};
	if (!transaction) {
		return false;
	}

	finishRemoval();
	return _protocol.readSectors(lba, data, count);
}

bool NativePicoSdTransport::write(std::uint32_t lba, std::uint8_t const* data, std::size_t count) {
	BusTransaction transaction{_queue};
	if (!transaction) {
		return false;
	}

	finishRemoval();
	return _protocol.writeSectors(lba, data, count);
}

bool NativePicoSdTransport::configured() {
	return _protocol.diagnosticStatus().configured;
}

bool NativePicoSdTransport::highCapacity() {
	return _protocol.diagnosticStatus().highCapacity;
}

std::uint32_t NativePicoSdTransport::clockMHz() {
	return platform::rk3506::picoSpi1ClockMHz();
}

bool NativePicoSdTransport::setClockMHz(std::uint32_t value) {
	BusTransaction transaction{_queue};
	if (!transaction) {
		return false;
	}

	finishRemoval();
	return platform::rk3506::picoSpi1SetClockMHz(value);
}

bool NativePicoSdTransport::dmaAvailable() {
	return platform::rk3506::picoSdDmaInfo().available;
}

bool NativePicoSdTransport::dmaEnabled() {
	return platform::rk3506::picoSdDmaEnabled();
}

} // namespace lexxos::devices::rk3506
