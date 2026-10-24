#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/devices/storage/sd_spi_backend.h>
#include <lexxos/devices/storage/sd_spi_protocol.h>

namespace lexxos::devices::storage::detail {

inline constexpr std::uint8_t cmd0GoIdle = 0;
inline constexpr std::uint8_t cmd8SendIfCond = 8;
inline constexpr std::uint8_t cmd9SendCsd = 9;
inline constexpr std::uint8_t cmd13SendStatus = 13;
inline constexpr std::uint8_t cmd16SetBlockLength = 16;
inline constexpr std::uint8_t cmd17ReadSingleBlock = 17;
inline constexpr std::uint8_t cmd18ReadMultipleBlock = 18;
inline constexpr std::uint8_t cmd24WriteSingleBlock = 24;
inline constexpr std::uint8_t cmd25WriteMultipleBlock = 25;
inline constexpr std::uint8_t cmd55ApplicationCommand = 55;
inline constexpr std::uint8_t cmd58ReadOcr = 58;
inline constexpr std::uint8_t acmd41SendOpCond = 41;

inline constexpr std::uint8_t r1Idle = 0x01;
inline constexpr std::uint8_t r1IllegalCommand = 0x04;
inline constexpr std::uint32_t hostCapacitySupport = 0x40000000;
inline constexpr std::uint32_t sdPowerUpDelayMs = 30;
inline constexpr std::uint8_t cmd0Attempts = 40;
inline constexpr std::uint8_t readRecoveryAttempts = 4;
inline constexpr std::uint8_t writeRecoveryAttempts = 2;
inline constexpr std::uint32_t recoveryDelayMs = 1;
inline constexpr bool defaultMultiBlockReadEnabled = true;
inline constexpr bool defaultMultiBlockWriteEnabled = false;
inline constexpr bool defaultHighSpeedWritesEnabled = true;
inline constexpr std::uint32_t transferRecoveryBaudRate = lexxos::devices::SdSpiBackend::safeTransferBaudRate;

#if defined(LEXXOS_SD_SPI_VALIDATE_READ_CRC_BY_DEFAULT)
inline constexpr bool defaultReadCrcValidation = LEXXOS_SD_SPI_VALIDATE_READ_CRC_BY_DEFAULT != 0;
#else
inline constexpr bool defaultReadCrcValidation = false;
#endif

#if defined(LEXXOS_SD_SPI_REINITIALIZE_ON_READ_FAILURE)
inline constexpr bool reinitializeOnReadFailure = LEXXOS_SD_SPI_REINITIALIZE_ON_READ_FAILURE != 0;
#else
inline constexpr bool reinitializeOnReadFailure = true;
#endif

[[nodiscard]] inline std::uint32_t u32FromBytes(std::uint8_t const* data) {
	return (static_cast<std::uint32_t>(data[0]) << 24)
	    | (static_cast<std::uint32_t>(data[1]) << 16)
	    | (static_cast<std::uint32_t>(data[2]) << 8)
	    | static_cast<std::uint32_t>(data[3]);
}

[[nodiscard]] inline std::uint16_t crc16Ccitt(std::uint8_t const* data, std::size_t size) {
	std::uint16_t crc = 0;
	for (std::size_t index = 0; index < size; ++index) {
		crc ^= static_cast<std::uint16_t>(data[index]) << 8;
		for (std::uint8_t bit = 0; bit < 8; ++bit) {
			crc = (crc & 0x8000U) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021U) : static_cast<std::uint16_t>(crc << 1);
		}
	}
	return crc;
}

[[nodiscard]] inline lexxos::devices::SdCardDiagnosticCode diagnosticCodeFor(lexxos::devices::SdSpiProtocolError error) {
	switch (error) {
		case lexxos::devices::SdSpiProtocolError::None:
			break;
		case lexxos::devices::SdSpiProtocolError::WaitReadyTimeout:
			return lexxos::devices::SdCardDiagnosticCode::WaitReadyTimeout;
		case lexxos::devices::SdSpiProtocolError::CommandWriteFailed:
			return lexxos::devices::SdCardDiagnosticCode::CommandWriteFailed;
		case lexxos::devices::SdSpiProtocolError::CommandResponseReadFailed:
			return lexxos::devices::SdCardDiagnosticCode::CommandResponseReadFailed;
		case lexxos::devices::SdSpiProtocolError::CommandResponseTimeout:
			return lexxos::devices::SdCardDiagnosticCode::CommandResponseTimeout;
		case lexxos::devices::SdSpiProtocolError::CommandExtraReadFailed:
			return lexxos::devices::SdCardDiagnosticCode::CommandExtraReadFailed;
	}
	return lexxos::devices::SdCardDiagnosticCode::TransferFailure;
}

} // namespace lexxos::devices::storage::detail
