#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::devices {

class BlockDevice {
public:
	virtual ~BlockDevice() = default;

	virtual bool initialize() = 0;
	[[nodiscard]] virtual bool isReady() const = 0;

	[[nodiscard]] virtual bool isReadOnly() const { return false; }

	[[nodiscard]] virtual std::uint32_t blockSizeBytes() const = 0;
	[[nodiscard]] virtual std::uint64_t blockCount() const = 0;
	virtual bool readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) = 0;
	virtual bool writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) = 0;
};

} // namespace lexxos::devices
