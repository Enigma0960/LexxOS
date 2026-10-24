#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/storage/block_device.h>

namespace lexxos::devices {

class AsyncBlockDevice final : public BlockDevice {
public:
	AsyncBlockDevice(BlockDevice& inner, char const* workerName, std::uint32_t stackSizeBytes, std::uint8_t priority);

	bool initialize() override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] bool isReadOnly() const override;
	[[nodiscard]] std::uint32_t blockSizeBytes() const override;
	[[nodiscard]] std::uint64_t blockCount() const override;
	bool readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) override;
	bool writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) override;

	[[nodiscard]] bool workerStarted() const;

private:
	enum class Operation : std::uint8_t {
		Initialize,
		Read,
		Write,
	};

	struct Request {
		Operation operation{Operation::Read};
		std::uint64_t firstBlock{0};
		void* buffer{nullptr};
		void const* constBuffer{nullptr};
		std::size_t blockCount{0};
		bool result{false};
		void* completion{nullptr};
	};

	[[nodiscard]] bool submit(Request& request);
	[[nodiscard]] bool executeInline(Request& request);
	[[nodiscard]] bool calledFromWorker() const;
	[[nodiscard]] bool shouldUseWorker();
	[[nodiscard]] bool ensureWorkerStarted();
	void runWorker();

	static void workerTrampoline(void* parameter);

	BlockDevice& _inner;
	char const* _workerName{nullptr};
	std::uint32_t _stackSizeBytes{0};
	std::uint8_t _priority{0};
	void* _queue{nullptr};
	void* _workerTask{nullptr};
	std::atomic<bool> _workerStartLock{false};
};

} // namespace lexxos::devices
