#include <lexxos/apps/storage/detail/block_read_benchmark.h>

#include <algorithm>
#include <cstring>

#include <lexxos/devices/storage/block_device.h>

namespace lexxos::apps::storage_programs::detail {

BlockReadBenchmarkResult BlockReadBenchmark::run(lexxos::devices::BlockDevice& device,
    BlockReadBenchmarkOptions const& options, std::span<std::uint8_t> reference,
    std::span<std::uint8_t> transfer) {
	BlockReadBenchmarkResult result{};
	auto const sectors = device.blockCount();
	auto const size = device.blockSizeBytes();
	if (!device.isReady() || options.blocks == 0 || options.batch == 0 || options.firstBlock >= sectors || options.blocks > sectors - options.firstBlock) {
		result.status = BlockReadBenchmarkStatus::InvalidRange;
		return result;
	}

	if (size == 0 || options.batch > reference.size() / size || options.batch > transfer.size() / size) {
		result.status = BlockReadBenchmarkStatus::InvalidBuffer;
		return result;
	}

	auto read = [&](std::uint64_t first, std::uint8_t* buffer, std::uint32_t count, bool single) {
		result.failedBlock = first;
		if (options.cancelled && options.cancelled(options.userData)) {
			result.status = BlockReadBenchmarkStatus::Cancelled;
			return false;
		}

		if (!device.readBlocks(first, buffer, count)) {
			result.status = BlockReadBenchmarkStatus::ReadError;
			return false;
		}

		result.bytesRead += std::uint64_t(count) * size;
		if (single) {
			++result.singleRequests;
		} else {
			++result.batchRequests;
		}

		if (options.afterRead) {
			options.afterRead(options.userData);
		}

		return true;
	};

	// Сравниваем каждый байт одиночных и пакетных чтений; последний пакет может быть неполным.
	for (std::uint32_t offset = 0; offset < options.blocks;) {
		auto const count = std::min(options.batch, options.blocks - offset);
		auto const first = options.firstBlock + offset;
		for (std::uint32_t i = 0; i < count; ++i) {
			if (!read(first + i, reference.data() + i * size, 1, true)) {
				return result;
			}
		}

		if (!read(first, transfer.data(), count, false)) {
			return result;
		}

		if (std::memcmp(reference.data(), transfer.data(), std::size_t(count) * size) != 0) {
			result.status = BlockReadBenchmarkStatus::Mismatch;
			return result;
		}

		offset += count;
	}

	result.status = options.cancelled && options.cancelled(options.userData)
	    ? BlockReadBenchmarkStatus::Cancelled
	    : BlockReadBenchmarkStatus::Ok;
	return result;
}

} // namespace lexxos::apps::storage_programs::detail
