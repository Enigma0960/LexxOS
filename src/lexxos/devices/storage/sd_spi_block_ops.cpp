#include <lexxos/devices/storage/sd_spi_block_ops.h>

#include <array>
#include <cstdint>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t cmd12StopTransmission = 12;
	constexpr std::uint8_t cmd13SendStatus = 13;
	constexpr std::uint8_t cmd17ReadSingleBlock = 17;
	constexpr std::uint8_t cmd18ReadMultipleBlock = 18;
	constexpr std::uint8_t cmd24WriteSingleBlock = 24;
	constexpr std::uint8_t cmd25WriteMultipleBlock = 25;
	constexpr std::uint8_t cmd55ApplicationCommand = 55;
	constexpr std::uint8_t acmd23SetWrBlkEraseCount = 23;

} // namespace

SdSpiBlockOps::SdSpiBlockOps(SdSpiTransport& transport, SdSpiProtocol& protocol)
    : _transport(transport)
    , _protocol(protocol) {
}

SdSpiBlockResult SdSpiBlockOps::readSingleBlock(std::uint32_t address, std::uint64_t block, std::uint8_t* buffer, SdSpiBlockTransferConfig config) {
	SdSpiCommandResult const command = _protocol.sendCommand(cmd17ReadSingleBlock, address, 0x01, true);
	if (command.r1 != 0x00) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd17Rejected, cmd17ReadSingleBlock, command.r1, block, 1);
	}

	if (!_protocol.waitDataToken(100000)) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::DataTokenTimeout, cmd17ReadSingleBlock, command.r1, block, 1);
	}

	SdSpiBlockResult const payload = readPayload(cmd17ReadSingleBlock, command.r1, block, buffer, config);
	_protocol.endTransaction();
	return payload;
}

SdSpiBlockResult SdSpiBlockOps::readMultipleBlocks(
    std::uint32_t address,
    std::uint64_t firstBlock,
    std::uint8_t* buffer,
    std::size_t blockCount,
    SdSpiBlockTransferConfig config) {
	if (blockCount == 0) {
		return success();
	}

	SdSpiCommandResult const command = _protocol.sendCommand(cmd18ReadMultipleBlock, address, 0x01, true);
	if (command.r1 != 0x00) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd18Rejected, cmd18ReadMultipleBlock, command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
	}

	std::uint8_t* previousBlock = nullptr;
	std::array<std::uint8_t, 2> previousCrc{};
	std::array<std::uint8_t, 2> crc{};
	for (std::size_t index = 0; index < blockCount; ++index) {
		std::uint64_t const currentBlock = firstBlock + index;
		if (!_protocol.waitDataToken(100000)) {
			(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
			return failure(SdCardDiagnosticCode::DataTokenTimeout, cmd18ReadMultipleBlock, command.r1, currentBlock, static_cast<std::uint32_t>(blockCount - index));
		}

		std::uint8_t* blockBuffer = buffer + index * blockSize;
		if (config.useDma && _transport.supportsDma()) {
			if (!_transport.startDmaTransfer(nullptr, blockBuffer, blockSize)) {
				(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
				return failure(SdCardDiagnosticCode::DmaStartFailed, cmd18ReadMultipleBlock, command.r1, currentBlock, 1);
			}

			bool const previousOk = previousBlock == nullptr || validateReadCrc(previousBlock, blockSize, previousCrc.data(), config.validateCrc);
			bool const transferOk = _transport.waitDmaTransfer(config.dmaTimeoutMs);
			if (!previousOk) {
				(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
				return failure(SdCardDiagnosticCode::ReadCrcMismatch, cmd18ReadMultipleBlock, command.r1, currentBlock - 1U, 1);
			}
			if (!transferOk) {
				(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
				return failure(SdCardDiagnosticCode::DmaWaitTimeout, cmd18ReadMultipleBlock, command.r1, currentBlock, 1);
			}
		} else {
			if (!_transport.read(0xFF, blockBuffer, blockSize)) {
				(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
				return failure(SdCardDiagnosticCode::RegisterReadFailed, cmd18ReadMultipleBlock, command.r1, currentBlock, 1);
			}
			if (previousBlock != nullptr && !validateReadCrc(previousBlock, blockSize, previousCrc.data(), config.validateCrc)) {
				(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
				return failure(SdCardDiagnosticCode::ReadCrcMismatch, cmd18ReadMultipleBlock, command.r1, currentBlock - 1U, 1);
			}
		}

		if (!_transport.read(0xFF, crc.data(), crc.size())) {
			(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
			return failure(SdCardDiagnosticCode::CrcReadFailed, cmd18ReadMultipleBlock, command.r1, currentBlock, 1);
		}

		previousBlock = blockBuffer;
		previousCrc = crc;
	}

	if (previousBlock != nullptr && !validateReadCrc(previousBlock, blockSize, previousCrc.data(), config.validateCrc)) {
		(void) stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
		return failure(SdCardDiagnosticCode::ReadCrcMismatch, cmd18ReadMultipleBlock, command.r1, firstBlock + blockCount - 1U, 1);
	}

	return stopMultipleBlockRead(command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
}

SdSpiBlockResult SdSpiBlockOps::writeSingleBlock(std::uint32_t address, std::uint64_t block, std::uint8_t const* buffer, SdSpiBlockTransferConfig config) {
	SdSpiCommandResult const command = _protocol.sendCommand(cmd24WriteSingleBlock, address, 0x01, true);
	if (command.r1 != 0x00) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd24Rejected, cmd24WriteSingleBlock, command.r1, block, 1);
	}

	SdSpiBlockResult const payload = writePayload(SdSpiProtocol::dataStartToken, cmd24WriteSingleBlock, command.r1, block, buffer, config);
	_protocol.endTransaction();
	return payload;
}

SdSpiBlockResult SdSpiBlockOps::writeMultipleBlocks(
    std::uint32_t address,
    std::uint64_t firstBlock,
    std::uint8_t const* buffer,
    std::size_t blockCount,
    SdSpiBlockTransferConfig config) {
	if (blockCount == 0) {
		return success();
	}

	(void) _protocol.sendCommand(cmd55ApplicationCommand, 0, 0x01, false);
	(void) _protocol.sendCommand(acmd23SetWrBlkEraseCount, static_cast<std::uint32_t>(blockCount), 0x01, false);
	SdSpiCommandResult const command = _protocol.sendCommand(cmd25WriteMultipleBlock, address, 0x01, true);
	if (command.r1 != 0x00) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd25Rejected, cmd25WriteMultipleBlock, command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
	}

	for (std::size_t index = 0; index < blockCount; ++index) {
		SdSpiBlockResult const payload = writePayload(SdSpiProtocol::multiWriteStartToken, cmd25WriteMultipleBlock, command.r1, firstBlock + index, buffer + index * blockSize, config);
		if (!payload.ok) {
			_protocol.endTransaction();
			return payload;
		}
	}

	bool const stopped = _protocol.writeByte(SdSpiProtocol::multiWriteStopToken) && _protocol.waitReady(1000000);
	_protocol.endTransaction();
	if (!stopped) {
		return failure(SdCardDiagnosticCode::MultiWriteStopFailed, cmd25WriteMultipleBlock, command.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
	}

	std::array<std::uint8_t, 2> status{};
	SdSpiCommandResult const statusCommand = _protocol.sendCommand(cmd13SendStatus, 0, 0x01, status.data(), status.size(), false);
	if (statusCommand.r1 != 0x00) {
		return failure(SdCardDiagnosticCode::Cmd13Rejected, cmd13SendStatus, statusCommand.r1, firstBlock, static_cast<std::uint32_t>(blockCount));
	}
	if (status[0] != 0x00 || status[1] != 0x00) {
		return failure(SdCardDiagnosticCode::Cmd13StatusError, cmd13SendStatus, static_cast<std::uint32_t>((status[0] << 8) | status[1]), firstBlock, static_cast<std::uint32_t>(blockCount));
	}
	return success();
}

SdSpiBlockResult SdSpiBlockOps::readPayload(std::uint8_t command, std::uint32_t r1, std::uint64_t block, std::uint8_t* buffer, SdSpiBlockTransferConfig config) {
	bool payloadRead = false;
	if (config.useDma && _transport.supportsDma()) {
		if (!_transport.startDmaTransfer(nullptr, buffer, blockSize)) {
			return failure(SdCardDiagnosticCode::DmaStartFailed, command, r1, block, 1);
		}
		if (!_transport.waitDmaTransfer(config.dmaTimeoutMs)) {
			return failure(SdCardDiagnosticCode::DmaWaitTimeout, command, r1, block, 1);
		}
		payloadRead = true;
	} else {
		payloadRead = _transport.read(0xFF, buffer, blockSize);
	}

	if (!payloadRead) {
		return failure(SdCardDiagnosticCode::RegisterReadFailed, command, r1, block, 1);
	}

	std::array<std::uint8_t, 2> crc{};
	if (!_transport.read(0xFF, crc.data(), crc.size())) {
		return failure(SdCardDiagnosticCode::CrcReadFailed, command, r1, block, 1);
	}
	if (!validateReadCrc(buffer, blockSize, crc.data(), config.validateCrc)) {
		return failure(SdCardDiagnosticCode::ReadCrcMismatch, command, r1, block, 1);
	}
	return success();
}

SdSpiBlockResult SdSpiBlockOps::writePayload(
    std::uint8_t token,
    std::uint8_t command,
    std::uint32_t r1,
    std::uint64_t block,
    std::uint8_t const* buffer,
    SdSpiBlockTransferConfig config) {
	if (!_protocol.writeByte(token)) {
		return failure(SdCardDiagnosticCode::WriteTokenFailed, command, r1, block, 1);
	}

	std::uint16_t const crcValue = config.validateCrc ? crc16Ccitt(buffer, blockSize) : 0xFFFFU;
	std::array<std::uint8_t, 2> crc{
	    static_cast<std::uint8_t>((crcValue >> 8) & 0xFFU),
	    static_cast<std::uint8_t>(crcValue & 0xFFU),
	};

	if (config.useDma && _transport.supportsDma()) {
		if (!_transport.startDmaTransfer(buffer, nullptr, blockSize)) {
			return failure(SdCardDiagnosticCode::WriteDmaStartFailed, command, r1, block, 1);
		}
		if (!_transport.waitDmaTransfer(config.dmaTimeoutMs)) {
			return failure(SdCardDiagnosticCode::WriteDmaWaitFailed, command, r1, block, 1);
		}
	} else if (!_transport.write(buffer, blockSize)) {
		return failure(SdCardDiagnosticCode::WriteDmaWaitFailed, command, r1, block, 1);
	}

	if (!_transport.write(crc.data(), crc.size())) {
		return failure(SdCardDiagnosticCode::WriteCrcWriteFailed, command, r1, block, 1);
	}

	std::uint8_t dataResponse = 0xFF;
	if (!_protocol.readByte(dataResponse)) {
		return failure(SdCardDiagnosticCode::WriteResponseReadFailed, command, r1, block, 1);
	}

	bool const accepted = (dataResponse & 0x1FU) == SdSpiProtocol::dataResponseAccepted;
	if (!accepted) {
		return failure(SdCardDiagnosticCode::WriteDataRejected, command, dataResponse, block, 1);
	}
	if (!_protocol.waitReady(1000000)) {
		return failure(SdCardDiagnosticCode::WriteBusyTimeout, command, r1, block, 1);
	}
	return success();
}

SdSpiBlockResult SdSpiBlockOps::stopMultipleBlockRead(std::uint8_t r1, std::uint64_t firstBlock, std::uint32_t blockCount) {
	std::array<std::uint8_t, 6> packet{
	    static_cast<std::uint8_t>(0x40U | cmd12StopTransmission),
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x01,
	};

	if (!_transport.write(packet.data(), packet.size())) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd12WriteFailed, cmd12StopTransmission, r1, firstBlock, blockCount);
	}

	std::uint8_t value = 0xFF;
	if (!_protocol.readByte(value)) {
		_protocol.endTransaction();
		return failure(SdCardDiagnosticCode::Cmd12ResponseReadFailed, cmd12StopTransmission, r1, firstBlock, blockCount);
	}

	std::uint8_t stopR1 = SdSpiProtocol::commandTimeoutResponse;
	for (std::uint8_t attempt = 0; attempt < 16; ++attempt) {
		if (!_protocol.readByte(stopR1)) {
			_protocol.endTransaction();
			return failure(SdCardDiagnosticCode::Cmd12ResponseReadFailed, cmd12StopTransmission, r1, firstBlock, blockCount);
		}
		if ((stopR1 & 0x80U) == 0) {
			break;
		}
	}

	bool const ready = _protocol.waitReady(100000);
	bool const ok = stopR1 == 0x00 && ready;
	_protocol.endTransaction();
	if (!ok) {
		return failure(ready ? SdCardDiagnosticCode::Cmd12Rejected : SdCardDiagnosticCode::Cmd12BusyTimeout, cmd12StopTransmission, stopR1, firstBlock, blockCount);
	}
	return success();
}

bool SdSpiBlockOps::validateReadCrc(std::uint8_t const* data, std::size_t size, std::uint8_t const* crc, bool enabled) const {
	if (!enabled) {
		return true;
	}
	std::uint16_t const expected = static_cast<std::uint16_t>((static_cast<std::uint16_t>(crc[0]) << 8) | crc[1]);
	return crc16Ccitt(data, size) == expected;
}

std::uint16_t SdSpiBlockOps::crc16Ccitt(std::uint8_t const* data, std::size_t size) const {
	std::uint16_t crc = 0;
	for (std::size_t index = 0; index < size; ++index) {
		crc ^= static_cast<std::uint16_t>(data[index]) << 8;
		for (std::uint8_t bit = 0; bit < 8; ++bit) {
			crc = (crc & 0x8000U) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021U) : static_cast<std::uint16_t>(crc << 1);
		}
	}
	return crc;
}

SdSpiBlockResult SdSpiBlockOps::success() const {
	return {
	    .ok = true,
	};
}

SdSpiBlockResult SdSpiBlockOps::failure(SdCardDiagnosticCode code, std::uint32_t command, std::uint32_t r1, std::uint64_t block, std::uint32_t blockCount) const {
	return {
	    .ok = false,
	    .code = code,
	    .command = command,
	    .r1 = r1,
	    .block = block,
	    .blockCount = blockCount,
	};
}

} // namespace lexxos::devices
