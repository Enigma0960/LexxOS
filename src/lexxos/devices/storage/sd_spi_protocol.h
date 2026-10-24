#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/sd_spi_transport.h>

namespace lexxos::devices {

enum class SdSpiProtocolError : std::uint8_t {
	None,
	WaitReadyTimeout,
	CommandWriteFailed,
	CommandResponseReadFailed,
	CommandResponseTimeout,
	CommandExtraReadFailed,
};

struct SdSpiCommandResult {
	std::uint8_t r1{0xFF};
	std::uint8_t diagnosticR1{0xFF};
	SdSpiProtocolError error{SdSpiProtocolError::None};
};

class SdSpiProtocol {
public:
	static constexpr std::uint8_t commandTimeoutResponse = 0xFF;
	static constexpr std::uint8_t cmd55ApplicationCommand = 55;
	static constexpr std::uint8_t dataStartToken = 0xFE;
	static constexpr std::uint8_t multiWriteStartToken = 0xFC;
	static constexpr std::uint8_t multiWriteStopToken = 0xFD;
	static constexpr std::uint8_t dataResponseAccepted = 0x05;

	explicit SdSpiProtocol(SdSpiTransport& transport);

	bool clockIdle(std::size_t byteCount);
	bool readByte(std::uint8_t& value);
	bool writeByte(std::uint8_t value);
	bool waitReady(std::uint32_t attempts);
	bool waitDataToken(std::uint32_t attempts);
	SdSpiCommandResult sendCommand(
	    std::uint8_t command,
	    std::uint32_t argument,
	    std::uint8_t crc,
	    std::uint8_t* response,
	    std::size_t responseSize,
	    bool keepSelected);
	SdSpiCommandResult sendCommand(std::uint8_t command, std::uint32_t argument, std::uint8_t crc, bool keepSelected = false);
	void endTransaction();

private:
	SdSpiTransport& _transport;
};

} // namespace lexxos::devices
