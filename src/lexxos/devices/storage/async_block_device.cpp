#include <lexxos/devices/storage/async_block_device.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <queue.h>
	#include <semphr.h>
	#include <task.h>
#endif

namespace lexxos::devices {

AsyncBlockDevice::AsyncBlockDevice(BlockDevice& inner, char const* workerName, std::uint32_t stackSizeBytes, std::uint8_t priority)
    : _inner(inner)
    , _workerName(workerName)
    , _stackSizeBytes(stackSizeBytes)
    , _priority(priority) {
}

bool AsyncBlockDevice::initialize() {
	if (!shouldUseWorker()) {
		return _inner.initialize();
	}

	Request request{
	    .operation = Operation::Initialize,
	};
	return submit(request);
}

bool AsyncBlockDevice::isReady() const {
	return _inner.isReady();
}

bool AsyncBlockDevice::isReadOnly() const {
	return _inner.isReadOnly();
}

std::uint32_t AsyncBlockDevice::blockSizeBytes() const {
	return _inner.blockSizeBytes();
}

std::uint64_t AsyncBlockDevice::blockCount() const {
	return _inner.blockCount();
}

bool AsyncBlockDevice::readBlocks(std::uint64_t firstBlock, void* buffer, std::size_t blockCount) {
	if (!shouldUseWorker()) {
		return _inner.readBlocks(firstBlock, buffer, blockCount);
	}

	Request request{
	    .operation = Operation::Read,
	    .firstBlock = firstBlock,
	    .buffer = buffer,
	    .blockCount = blockCount,
	};
	return submit(request);
}

bool AsyncBlockDevice::writeBlocks(std::uint64_t firstBlock, void const* buffer, std::size_t blockCount) {
	if (!shouldUseWorker()) {
		return _inner.writeBlocks(firstBlock, buffer, blockCount);
	}

	Request request{
	    .operation = Operation::Write,
	    .firstBlock = firstBlock,
	    .constBuffer = buffer,
	    .blockCount = blockCount,
	};
	return submit(request);
}

bool AsyncBlockDevice::workerStarted() const {
	return _workerTask != nullptr;
}

bool AsyncBlockDevice::submit(Request& request) {
#if defined(LEXXOS_KERNEL_FREERTOS)
	auto* completion = xSemaphoreCreateBinary();
	if (completion == nullptr) {
		return executeInline(request);
	}

	request.completion = completion;
	auto* requestPointer = &request;
	bool queued = xQueueSend(static_cast<QueueHandle_t>(_queue), &requestPointer, portMAX_DELAY) == pdTRUE;
	if (queued) {
		queued = xSemaphoreTake(completion, portMAX_DELAY) == pdTRUE;
	}
	vSemaphoreDelete(completion);
	return queued && request.result;
#else
	return executeInline(request);
#endif
}

bool AsyncBlockDevice::executeInline(Request& request) {
	switch (request.operation) {
		case Operation::Initialize:
			return _inner.initialize();
		case Operation::Read:
			return _inner.readBlocks(request.firstBlock, request.buffer, request.blockCount);
		case Operation::Write:
			return _inner.writeBlocks(request.firstBlock, request.constBuffer, request.blockCount);
	}
	return false;
}

bool AsyncBlockDevice::calledFromWorker() const {
#if defined(LEXXOS_KERNEL_FREERTOS)
	return _workerTask != nullptr && xTaskGetCurrentTaskHandle() == static_cast<TaskHandle_t>(_workerTask);
#else
	return false;
#endif
}

bool AsyncBlockDevice::shouldUseWorker() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
		return false;
	}
	if (calledFromWorker()) {
		return false;
	}
	return ensureWorkerStarted();
#else
	return false;
#endif
}

bool AsyncBlockDevice::ensureWorkerStarted() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_workerTask != nullptr && _queue != nullptr) {
		return true;
	}

	while (_workerStartLock.exchange(true, std::memory_order_acquire)) {
		taskYIELD();
	}

	if (_workerTask != nullptr && _queue != nullptr) {
		_workerStartLock.store(false, std::memory_order_release);
		return true;
	}

	if (_queue == nullptr) {
		_queue = xQueueCreate(2, sizeof(Request*));
		if (_queue == nullptr) {
			_workerStartLock.store(false, std::memory_order_release);
			return false;
		}
	}

	TaskHandle_t workerTask = nullptr;
	BaseType_t const created = xTaskCreate(workerTrampoline,
	    _workerName == nullptr ? "block-io" : _workerName,
	    static_cast<configSTACK_DEPTH_TYPE>(_stackSizeBytes / sizeof(StackType_t)),
	    this,
	    static_cast<UBaseType_t>(_priority),
	    &workerTask);
	if (created != pdPASS) {
		_workerStartLock.store(false, std::memory_order_release);
		return false;
	}

	_workerTask = workerTask;
	#if defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(LEXXOS_SD_IO_WORKER_CORE) && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > LEXXOS_SD_IO_WORKER_CORE)
	vTaskCoreAffinitySet(workerTask, 1U << LEXXOS_SD_IO_WORKER_CORE);
	#endif
	_workerStartLock.store(false, std::memory_order_release);
	return true;
#else
	return false;
#endif
}

void AsyncBlockDevice::runWorker() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	while (true) {
		Request* request = nullptr;
		if (xQueueReceive(static_cast<QueueHandle_t>(_queue), &request, portMAX_DELAY) != pdTRUE || request == nullptr) {
			continue;
		}

		request->result = executeInline(*request);

		if (request->completion != nullptr) {
			(void) xSemaphoreGive(static_cast<SemaphoreHandle_t>(request->completion));
		}
	}
#endif
}

void AsyncBlockDevice::workerTrampoline(void* parameter) {
	auto& device = *static_cast<AsyncBlockDevice*>(parameter);
	device.runWorker();
}

} // namespace lexxos::devices
