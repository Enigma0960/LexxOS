#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/devices/storage/sd_spi_protocol.h>
#include <lexxos/devices/storage/sd_spi_transport.h>

namespace lexxos::devices {

struct SdSpiBlockTransferConfig {
	bool useDma{false};
	bool validateCrc{false};
	std::uint32_t dmaTimeoutMs{1000};
};

struct SdSpiBlockResult {
	bool ok{false};
	SdCardDiagnosticCode code{SdCardDiagnosticCode::None};
	std::uint32_t command{0xFFU};
	std::uint32_t r1{0xFFFFFFFFU};
	std::uint64_t block{0};
	std::uint32_t blockCount{0};
};

class SdSpiBlockOps {
public:
	static constexpr std::uint32_t blockSize = 512;

	explicit SdSpiBlockOps(SdSpiTransport& transport, SdSpiProtocol& protocol);

	[[nodiscard]] SdSpiBlockResult readSingleBlock(std::uint32_t address, std::uint64_t block, std::uint8_t* buffer, SdSpiBlockTransferConfig config);
	[[nodiscard]] SdSpiBlockResult readMultipleBlocks(
	    std::uint32_t address,
	    std::uint64_t firstBlock,
	    std::uint8_t* buffer,
	    std::size_t blockCount,
	    SdSpiBlockTransferConfig config);
	[[nodiscard]] SdSpiBlockResult writeSingleBlock(std::uint32_t address, std::uint64_t block, std::uint8_t const* buffer, SdSpiBlockTransferConfig config);
	[[nodiscard]] SdSpiBlockResult writeMultipleBlocks(
	    std::uint32_t address,
	    std::uint64_t firstBlock,
	    std::uint8_t const* buffer,
	    std::size_t blockCount,
	    SdSpiBlockTransferConfig config);

private:
	[[nodiscard]] SdSpiBlockResult readPayload(std::uint8_t command, std::uint32_t r1, std::uint64_t block, std::uint8_t* buffer, SdSpiBlockTransferConfig config);
	[[nodiscard]] SdSpiBlockResult writePayload(
	    std::uint8_t token,
	    std::uint8_t command,
	    std::uint32_t r1,
	    std::uint64_t block,
	    std::uint8_t const* buffer,
	    SdSpiBlockTransferConfig config);
	[[nodiscard]] SdSpiBlockResult stopMultipleBlockRead(std::uint8_t r1, std::uint64_t firstBlock, std::uint32_t blockCount);
	[[nodiscard]] bool validateReadCrc(std::uint8_t const* data, std::size_t size, std::uint8_t const* crc, bool enabled) const;
	[[nodiscard]] std::uint16_t crc16Ccitt(std::uint8_t const* data, std::size_t size) const;
	[[nodiscard]] SdSpiBlockResult success() const;
	[[nodiscard]] SdSpiBlockResult failure(SdCardDiagnosticCode code, std::uint32_t command, std::uint32_t r1, std::uint64_t block, std::uint32_t blockCount) const;

	SdSpiTransport& _transport;
	SdSpiProtocol& _protocol;
};

} // namespace lexxos::devices
