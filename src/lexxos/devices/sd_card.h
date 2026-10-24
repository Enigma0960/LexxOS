#pragma once

#include <cstdint>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/devices/storage/sd_card_backend.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices::sd_card {

bool initialize();
bool initialize(lexxos::platform::PlatformServices const& platform);
bool probe(std::uint8_t attempts = 3);
bool probe(lexxos::platform::PlatformServices const& platform, std::uint8_t attempts = 3);
[[nodiscard]] bool isReady();
[[nodiscard]] SdCardInterface interfaceType();
[[nodiscard]] lexxos::platform::SdInterface boardInterface();
[[nodiscard]] lexxos::platform::SdInterface boardInterface(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool backendMatchesBoardInterface();
[[nodiscard]] bool backendMatchesBoardInterface(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] SdCardState state();
[[nodiscard]] SdCardType cardType();
[[nodiscard]] std::uint32_t blockSizeBytes();
[[nodiscard]] std::uint64_t blockCount();
[[nodiscard]] std::uint32_t activeBaudRate();
[[nodiscard]] SdTimingProfile timingProfile();
bool setTransferBaudRate(std::uint32_t baudRate);
bool setSpiClockMode(SpiClockMode mode);
[[nodiscard]] SpiClockMode spiClockMode();
void setReadCrcValidation(bool enabled);
void setMultiBlockRead(bool enabled);
[[nodiscard]] bool multiBlockReadEnabled();
void setMultiBlockWrite(bool enabled);
[[nodiscard]] bool multiBlockWriteEnabled();
[[nodiscard]] bool dmaSupported();
void setDmaTransfersEnabled(bool enabled);
[[nodiscard]] bool dmaTransfersEnabled();
void setHighSpeedWritesEnabled(bool enabled);
[[nodiscard]] bool highSpeedWritesEnabled();
void setDiagnosticsEnabled(bool enabled);
[[nodiscard]] bool diagnosticsEnabled();
void clearDiagnostics();
[[nodiscard]] SdCardDiagnostics diagnostics();
[[nodiscard]] bool readCrcValidation();
[[nodiscard]] std::uint32_t readRecoveryReinitializations();
[[nodiscard]] bool hasCardDetect();
[[nodiscard]] bool isCardInserted();
[[nodiscard]] BlockDevice* blockDevice();
[[nodiscard]] BlockDevice* blockDevice(lexxos::platform::PlatformServices const& platform);

} // namespace lexxos::devices::sd_card
