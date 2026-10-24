#include <lexxos/devices/storage/sd_card_backend.h>

namespace lexxos::devices {

char const* toString(SdCardInterface interfaceType) {
	switch (interfaceType) {
		case SdCardInterface::Unknown:
			return "unknown";
		case SdCardInterface::Spi:
			return "spi";
		case SdCardInterface::Sdio:
			return "sdio";
		case SdCardInterface::Simulated:
			return "sim";
	}
	return "unknown";
}

char const* toString(SdCardState state) {
	switch (state) {
		case SdCardState::Uninitialized:
			return "uninitialized";
		case SdCardState::Ready:
			return "ready";
		case SdCardState::NoBus:
			return "no-bus";
		case SdCardState::NoCard:
			return "no-card";
		case SdCardState::Unsupported:
			return "unsupported";
		case SdCardState::Timeout:
			return "timeout";
		case SdCardState::IoError:
			return "io-error";
		case SdCardState::CardRemoved:
			return "card-removed";
	}
	return "unknown";
}

char const* toString(SdCardType type) {
	switch (type) {
		case SdCardType::Unknown:
			return "unknown";
		case SdCardType::SdV1:
			return "sd-v1";
		case SdCardType::SdV2ByteAddressed:
			return "sd-v2-byte";
		case SdCardType::SdV2BlockAddressed:
			return "sd-v2-block";
	}
	return "unknown";
}

char const* toString(SdCardDiagnosticCode code) {
	switch (code) {
		case SdCardDiagnosticCode::None:
			return "none";
		case SdCardDiagnosticCode::LockFailed:
			return "lock failed";
		case SdCardDiagnosticCode::InitializeFailed:
			return "initialize failed";
		case SdCardDiagnosticCode::SdioPowerControlFailed:
			return "SDIO power control failed";
		case SdCardDiagnosticCode::SdioHostInitFailed:
			return "SDIO host init failed";
		case SdCardDiagnosticCode::SdioSlotInitFailed:
			return "SDIO slot init failed";
		case SdCardDiagnosticCode::SdioCardInitFailed:
			return "SDIO card init failed";
		case SdCardDiagnosticCode::NotReady:
			return "not ready";
		case SdCardDiagnosticCode::NullBuffer:
			return "null buffer";
		case SdCardDiagnosticCode::CardRemoved:
			return "card removed";
		case SdCardDiagnosticCode::SetBaudFailed:
			return "set baud failed";
		case SdCardDiagnosticCode::SetModeFailed:
			return "set mode failed";
		case SdCardDiagnosticCode::WaitReadyTimeout:
			return "wait ready timeout";
		case SdCardDiagnosticCode::CommandWriteFailed:
			return "command write failed";
		case SdCardDiagnosticCode::CommandResponseTimeout:
			return "command response timeout";
		case SdCardDiagnosticCode::CommandResponseReadFailed:
			return "command response read failed";
		case SdCardDiagnosticCode::CommandExtraReadFailed:
			return "command extra response read failed";
		case SdCardDiagnosticCode::RegisterTokenTimeout:
			return "register token timeout";
		case SdCardDiagnosticCode::RegisterReadFailed:
			return "register read failed";
		case SdCardDiagnosticCode::DataTokenTimeout:
			return "data token timeout";
		case SdCardDiagnosticCode::DmaStartFailed:
			return "dma start failed";
		case SdCardDiagnosticCode::DmaWaitTimeout:
			return "dma wait timeout";
		case SdCardDiagnosticCode::CrcReadFailed:
			return "crc read failed";
		case SdCardDiagnosticCode::ReadCrcMismatch:
			return "read crc mismatch";
		case SdCardDiagnosticCode::Cmd17Rejected:
			return "CMD17 rejected";
		case SdCardDiagnosticCode::Cmd18Rejected:
			return "CMD18 rejected";
		case SdCardDiagnosticCode::Cmd12WriteFailed:
			return "CMD12 write failed";
		case SdCardDiagnosticCode::Cmd12ResponseReadFailed:
			return "CMD12 response read failed";
		case SdCardDiagnosticCode::Cmd12Rejected:
			return "CMD12 rejected";
		case SdCardDiagnosticCode::Cmd12BusyTimeout:
			return "CMD12 busy timeout";
		case SdCardDiagnosticCode::WriteNotAllowed:
			return "write not allowed";
		case SdCardDiagnosticCode::WriteSpeedGuard:
			return "write speed guard";
		case SdCardDiagnosticCode::Cmd24Rejected:
			return "CMD24 rejected";
		case SdCardDiagnosticCode::WriteTokenFailed:
			return "write token failed";
		case SdCardDiagnosticCode::WriteDmaStartFailed:
			return "write dma start failed";
		case SdCardDiagnosticCode::WriteDmaWaitFailed:
			return "write dma wait failed";
		case SdCardDiagnosticCode::WriteCrcWriteFailed:
			return "write crc write failed";
		case SdCardDiagnosticCode::WriteResponseReadFailed:
			return "write response read failed";
		case SdCardDiagnosticCode::WriteDataRejected:
			return "write data rejected";
		case SdCardDiagnosticCode::WriteBusyTimeout:
			return "write busy timeout";
		case SdCardDiagnosticCode::Cmd25Rejected:
			return "CMD25 rejected";
		case SdCardDiagnosticCode::MultiWriteStopFailed:
			return "multi-write stop failed";
		case SdCardDiagnosticCode::Cmd13Rejected:
			return "CMD13 rejected";
		case SdCardDiagnosticCode::Cmd13StatusError:
			return "CMD13 status error";
		case SdCardDiagnosticCode::ReadFallbackToSafeBaud:
			return "read fallback to safe baud";
		case SdCardDiagnosticCode::WriteFallbackToSafeBaud:
			return "write fallback to safe baud";
		case SdCardDiagnosticCode::ReinitializeFailed:
			return "reinitialize failed";
		case SdCardDiagnosticCode::TransferFailure:
			return "transfer failure";
	}
	return "unknown";
}

SdTimingProfile SdCardBackend::timingProfile() const {
	return {};
}

bool SdCardBackend::setTransferBaudRate(std::uint32_t baudRate) {
	(void) baudRate;
	return false;
}

bool SdCardBackend::setSpiClockMode(SpiClockMode mode) {
	(void) mode;
	return false;
}

SpiClockMode SdCardBackend::spiClockMode() const {
	return SpiClockMode::Mode0;
}

void SdCardBackend::setReadCrcValidation(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::readCrcValidation() const {
	return false;
}

void SdCardBackend::setMultiBlockRead(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::multiBlockReadEnabled() const {
	return false;
}

void SdCardBackend::setMultiBlockWrite(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::multiBlockWriteEnabled() const {
	return false;
}

bool SdCardBackend::dmaSupported() const {
	return false;
}

void SdCardBackend::setDmaTransfersEnabled(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::dmaTransfersEnabled() const {
	return false;
}

void SdCardBackend::setHighSpeedWritesEnabled(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::highSpeedWritesEnabled() const {
	return false;
}

void SdCardBackend::setDiagnosticsEnabled(bool enabled) {
	(void) enabled;
}

bool SdCardBackend::diagnosticsEnabled() const {
	return false;
}

void SdCardBackend::clearDiagnostics() {
}

SdCardDiagnostics SdCardBackend::diagnostics() {
	return {};
}

std::uint32_t SdCardBackend::readRecoveryReinitializations() const {
	return 0;
}

} // namespace lexxos::devices
