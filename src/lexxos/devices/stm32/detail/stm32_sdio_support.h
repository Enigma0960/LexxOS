#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/platform/board.h>

namespace lexxos::devices::stm32::detail {

enum class SdioResponseType : std::uint8_t {
	None,
	Short,
	ShortNoCrc,
	Long,
};

struct SdioCommandResult {
	bool ok{false};
	std::uint32_t response[4]{};
};

[[nodiscard]] bool configureSdioPeripheral(lexxos::platform::SdioPins const& pins);
[[nodiscard]] bool setSdioClock(std::uint32_t requestedClockHz, std::uint32_t& activeClockHz);
void configureSdioOneBitBus();
[[nodiscard]] SdioCommandResult sendSdioCommand(
    std::uint8_t command,
    std::uint32_t argument,
    SdioResponseType responseType);
void prepareSdioBlockRead();
void prepareSdioBlockWrite();
void cancelSdioDataTransfer();
[[nodiscard]] bool finishSdioBlockRead(std::uint8_t* buffer, std::size_t size);
[[nodiscard]] bool finishSdioBlockWrite(std::uint8_t const* buffer, std::size_t size);
[[nodiscard]] std::uint32_t readCsdBits(std::uint32_t const response[4], int highBit, int lowBit);

} // namespace lexxos::devices::stm32::detail
