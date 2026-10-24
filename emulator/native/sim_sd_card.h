#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/sd_card_backend.h>

namespace lexxos::emulator::detail {

class SimSdCardBlockDevice final : public lexxos::devices::SdCardBackend {
public:
	bool initialize() override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] bool isReadOnly() const override;
	[[nodiscard]] std::uint32_t blockSizeBytes() const override;
	[[nodiscard]] std::uint64_t blockCount() const override;
	bool readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t count) override;
	bool writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t count) override;

	[[nodiscard]] lexxos::devices::SdCardInterface interfaceType() const override;
	[[nodiscard]] lexxos::devices::SdCardState state() const override;
	[[nodiscard]] lexxos::devices::SdCardType cardType() const override;
	[[nodiscard]] std::uint32_t activeClockHz() const override;
	[[nodiscard]] bool hasCardDetect() const override;
	[[nodiscard]] bool isCardInserted() const override;

private:
	[[nodiscard]] bool inserted() const;

	static constexpr std::uint8_t deviceId = 0;

	mutable lexxos::devices::SdCardState _state{lexxos::devices::SdCardState::Uninitialized};
	std::uint64_t _blockCount{0};
};

} // namespace lexxos::emulator::detail
