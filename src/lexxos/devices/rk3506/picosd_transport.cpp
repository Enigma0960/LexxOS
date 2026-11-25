// Compatibility entrypoints share one product-owned removable SD session.
#include <lexxos/composition/products/lyra_sd_protocol.h>
#include <lexxos/devices/rk3506/sd_protocol.h>

namespace lexxos::platform::rk3506 {

PicoSdCrcMode picoSdCrcMode() {
	return composition::products::lyraSdProtocol().crcMode();
}

bool picoSdSetCrcMode(PicoSdCrcMode mode) {
	return composition::products::lyraSdProtocol().setCrcMode(mode);
}

PicoSdReadProfile picoSdReadProfileStatistics() {
	return composition::products::lyraSdProtocol().profileStatistics();
}

bool picoSdResetReadProfileStatistics() {
	return composition::products::lyraSdProtocol().resetProfileStatistics();
}

bool picoSdCardInserted() {
	return composition::products::lyraSdProtocol().cardInserted();
}

void picoSdInvalidateRemoved() {
	composition::products::lyraSdProtocol().invalidateRemoved();
}

bool picoSdProbeAuto() {
	return composition::products::lyraSdProtocol().probeAuto();
}

bool picoSdReady() {
	return composition::products::lyraSdProtocol().ready();
}

std::uint64_t picoSdSectorCount() {
	return composition::products::lyraSdProtocol().sectorCount();
}

bool picoSdReadSector(std::uint32_t lba, std::uint8_t* destination) {
	return composition::products::lyraSdProtocol().readSector(lba, destination);
}

bool picoSdReadSectors(std::uint32_t lba, std::uint8_t* destination, std::size_t count) {
	return composition::products::lyraSdProtocol().readSectors(lba, destination, count);
}

bool picoSdWriteSectors(std::uint32_t lba, std::uint8_t const* source, std::size_t count) {
	return composition::products::lyraSdProtocol().writeSectors(lba, source, count);
}

void picoSdSetMediaHooks(PicoSdMediaHooks hooks) {
	composition::products::lyraSdProtocol().setMediaHooks(hooks);
}

PicoSdDiagnosticStatus picoSdDiagnosticStatus() {
	return composition::products::lyraSdProtocol().diagnosticStatus();
}

PicoSdRegisterStatus picoSdRegisterStatus() {
	return composition::products::lyraSdProtocol().registerStatus();
}

bool picoSdSelectTransport(bool spi1) {
	return composition::products::lyraSdProtocol().selectTransport(spi1);
}

bool picoSdProbeManual() {
	return composition::products::lyraSdProtocol().probeManual();
}

} // namespace lexxos::platform::rk3506
