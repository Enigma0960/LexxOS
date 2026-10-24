#include <lexxos/devices/storage/sd_spi_protocol.h>

#include <algorithm>
#include <array>

namespace lexxos::devices {
SdSpiProtocol::SdSpiProtocol(SdSpiTransport& transport)
    : _transport(transport) {
}

bool SdSpiProtocol::clockIdle(std::size_t byteCount) {
	std::array<std::uint8_t, 16> buffer{};
	std::size_t remaining = byteCount;
	while (remaining > 0) {
		std::size_t const chunk = std::min(remaining, buffer.size());
		if (!_transport.read(0xFF, buffer.data(), chunk)) {
			return false;
		}
		remaining -= chunk;
	}
	return true;
}

bool SdSpiProtocol::readByte(std::uint8_t& value) {
	return _transport.read(0xFF, &value, 1);
}

bool SdSpiProtocol::writeByte(std::uint8_t value) {
	return _transport.write(&value, 1);
}

bool SdSpiProtocol::waitReady(std::uint32_t attempts) {
	std::uint8_t value = 0;
	for (std::uint32_t attempt = 0; attempt < attempts; ++attempt) {
		if (!readByte(value)) {
			return false;
		}
		if (value == 0xFF) {
			return true;
		}
	}
	return false;
}

bool SdSpiProtocol::waitDataToken(std::uint32_t attempts) {
	std::uint8_t value = 0;
	for (std::uint32_t attempt = 0; attempt < attempts; ++attempt) {
		if (!readByte(value)) {
			return false;
		}
		if (value == dataStartToken) {
			return true;
		}
	}
	return false;
}

SdSpiCommandResult SdSpiProtocol::sendCommand(
    std::uint8_t command,
    std::uint32_t argument,
    std::uint8_t crc,
    std::uint8_t* response,
    std::size_t responseSize,
    bool keepSelected) {
	_transport.deselect();
	clockIdle(1);
	_transport.select();

	if (!waitReady(10000)) {
		endTransaction();
		return {
		    .r1 = commandTimeoutResponse,
		    .diagnosticR1 = commandTimeoutResponse,
		    .error = SdSpiProtocolError::WaitReadyTimeout,
		};
	}

	std::array<std::uint8_t, 6> packet{
	    static_cast<std::uint8_t>(0x40U | command),
	    static_cast<std::uint8_t>((argument >> 24) & 0xFFU),
	    static_cast<std::uint8_t>((argument >> 16) & 0xFFU),
	    static_cast<std::uint8_t>((argument >> 8) & 0xFFU),
	    static_cast<std::uint8_t>(argument & 0xFFU),
	    static_cast<std::uint8_t>(crc | 0x01U),
	};

	if (!_transport.write(packet.data(), packet.size())) {
		endTransaction();
		return {
		    .r1 = commandTimeoutResponse,
		    .diagnosticR1 = commandTimeoutResponse,
		    .error = SdSpiProtocolError::CommandWriteFailed,
		};
	}

	std::uint8_t r1 = commandTimeoutResponse;
	for (std::uint8_t attempt = 0; attempt < 16; ++attempt) {
		if (!readByte(r1)) {
			endTransaction();
			return {
			    .r1 = commandTimeoutResponse,
			    .diagnosticR1 = commandTimeoutResponse,
			    .error = SdSpiProtocolError::CommandResponseReadFailed,
			};
		}
		if ((r1 & 0x80U) == 0) {
			break;
		}
	}

	SdSpiProtocolError error = SdSpiProtocolError::None;
	if (r1 == commandTimeoutResponse) {
		error = SdSpiProtocolError::CommandResponseTimeout;
	}

	if (r1 != commandTimeoutResponse && response != nullptr && responseSize > 0) {
		if (!_transport.read(0xFF, response, responseSize)) {
			endTransaction();
			return {
			    .r1 = commandTimeoutResponse,
			    .diagnosticR1 = r1,
			    .error = SdSpiProtocolError::CommandExtraReadFailed,
			};
		}
	}

	if (!keepSelected) {
		endTransaction();
	}
	return {
	    .r1 = r1,
	    .diagnosticR1 = r1,
	    .error = error,
	};
}

SdSpiCommandResult SdSpiProtocol::sendCommand(std::uint8_t command, std::uint32_t argument, std::uint8_t crc, bool keepSelected) {
	return sendCommand(command, argument, crc, nullptr, 0, keepSelected);
}

void SdSpiProtocol::endTransaction() {
	_transport.deselect();
	clockIdle(1);
}

} // namespace lexxos::devices
