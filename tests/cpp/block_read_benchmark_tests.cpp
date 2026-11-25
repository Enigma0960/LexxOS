#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#include <lexxos/apps/storage/detail/block_read_benchmark.h>
#include <lexxos/devices/storage/block_device.h>

namespace {

void expect(bool ok, char const* reason) {
	if (!ok) {
		std::cerr << reason << '\n';
		std::exit(1);
	}
}

class TestDisk final : public lexxos::devices::BlockDevice {
public:
	explicit TestDisk(unsigned seed) {
		for (std::size_t i = 0; i < data.size(); ++i) {
			data[i] = static_cast<std::uint8_t>((i / 512U) * 17U + i + seed);
		}
	}

	bool initialize() override { return ready; }

	bool isReady() const override { return ready; }

	bool isReadOnly() const override { return true; }

	std::uint32_t blockSizeBytes() const override { return sectorSize; }

	std::uint64_t blockCount() const override { return data.size() / 512; }

	bool readBlocks(std::uint64_t first, void* out, std::size_t count) override {
		requests.push_back(count);
		if (fail || !ready || first >= blockCount() || count > blockCount() - first) {
			return false;
		}

		std::memcpy(out, data.data() + first * 512, count * 512);
		if (corrupt && count > 1) {
			static_cast<std::uint8_t*>(out)[512] ^= 1;
		}

		return true;
	}

	bool writeBlocks(std::uint64_t, void const*, std::size_t) override {
		++writes;
		return false;
	}

	std::array<std::uint8_t, 32 * 512> data{};
	std::vector<std::size_t> requests;
	unsigned writes{};
	std::uint32_t sectorSize{512};
	bool ready{true}, fail{}, corrupt{};
};

} // namespace

int main() {
	using lexxos::apps::storage_programs::detail::BlockReadBenchmark;
	using lexxos::apps::storage_programs::detail::BlockReadBenchmarkOptions;
	using lexxos::apps::storage_programs::detail::BlockReadBenchmarkStatus;
	std::array<std::uint8_t, 4096> reference{}, transfer{};
	TestDisk first{3}, second{197};
	for (auto* disk : {&first, &second}) {
		for (auto batch : {1U, 2U, 3U, 8U}) {
			BlockReadBenchmarkOptions options{.firstBlock = 2, .blocks = 17, .batch = batch};
			auto result = BlockReadBenchmark::run(*disk, options, reference, transfer);
			expect(result.status == BlockReadBenchmarkStatus::Ok, "single/multi-block comparison");
			expect(result.singleRequests == 17 && result.batchRequests == (17 + batch - 1) / batch, "tail batch counted");
			expect(result.bytesRead == 17 * 512 * 2, "exact I/O accounting");
			expect(disk->writes == 0, "benchmark never writes raw sectors");
		}
	}
	BlockReadBenchmarkOptions options{.firstBlock = 31, .blocks = 1, .batch = 8};
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::Ok, "last sector");
	options.blocks = 2;
	auto calls = first.requests.size();
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::InvalidRange && first.requests.size() == calls, "reject crossing end before I/O");
	options.firstBlock = std::numeric_limits<std::uint64_t>::max();
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::InvalidRange, "overflow-safe LBA validation");
	options.firstBlock = 0;
	options.blocks = 17;
	expect(BlockReadBenchmark::run(first, options, std::span(reference).first(511), transfer).status == BlockReadBenchmarkStatus::InvalidBuffer, "small workspace");
	first.corrupt = true;
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::Mismatch, "detect corruption outside first block");
	first.corrupt = false;
	first.fail = true;
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::ReadError, "transport failure");
	first.fail = false;
	first.ready = false;
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::InvalidRange, "removed device");
	first.ready = true;
	options.cancelled = [](void*) {
		return true;
	};
	calls = first.requests.size();
	expect(BlockReadBenchmark::run(first, options, reference, transfer).status == BlockReadBenchmarkStatus::Cancelled && calls == first.requests.size(), "cancellation before I/O");
}
