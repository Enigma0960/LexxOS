#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace lexxos::devices {

class BlockDevice;

} // namespace lexxos::devices

namespace lexxos::apps::storage_programs::detail {

struct BlockReadBenchmarkOptions {
	std::uint64_t firstBlock{0};
	std::uint32_t blocks{128};
	std::uint32_t batch{8};
	bool (*cancelled)(void*){nullptr};
	void (*afterRead)(void*){nullptr};
	void* userData{nullptr};
};
enum class BlockReadBenchmarkStatus {
	Ok,
	InvalidRange,
	InvalidBuffer,
	ReadError,
	Mismatch,
	Cancelled
};

struct BlockReadBenchmarkResult {
	BlockReadBenchmarkStatus status{BlockReadBenchmarkStatus::Ok};
	std::uint64_t failedBlock{0};
	std::uint64_t bytesRead{0};
	std::uint32_t singleRequests{0};
	std::uint32_t batchRequests{0};
};

class BlockReadBenchmark final {
public:
	static BlockReadBenchmarkResult run(lexxos::devices::BlockDevice& device,
	    BlockReadBenchmarkOptions const& options, std::span<std::uint8_t> reference,
	    std::span<std::uint8_t> transfer);
};

} // namespace lexxos::apps::storage_programs::detail
