#pragma once

#include <cstdint>

namespace lexxos::apps::storage_programs::detail {

enum class SpeedScanScope : std::uint8_t {
	ReadOnly,
	ReadWrite,
};

struct SdSpeedScanConfig {
	std::uint32_t maxHz{0};
	std::uint32_t stepHz{0};
	std::uint32_t blocksPerFile{0};
	std::uint32_t files{0};
	std::uint32_t rawBatchBlocks{0};
	std::uint64_t rawStartBlock{0};
	bool dmaTransfersEnabled{false};
	bool highSpeedWritesEnabled{false};
	SpeedScanScope scope{SpeedScanScope::ReadOnly};
};

[[nodiscard]] constexpr std::uint32_t rawBlockCount(SdSpeedScanConfig const& config) {
	return config.blocksPerFile * config.files;
}

[[nodiscard]] constexpr std::uint64_t fatByteCount(SdSpeedScanConfig const& config) {
	return static_cast<std::uint64_t>(config.blocksPerFile) * config.files * 512ULL;
}

} // namespace lexxos::apps::storage_programs::detail
