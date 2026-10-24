#include <lexxos/program/program_memory_arena.h>

#include <cstdlib>

#include <lexxos/feature.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <portable.h>
#endif

namespace lexxos::program {
namespace {

	void* allocateRaw(std::size_t size) {
		if (size == 0) {
			return nullptr;
		}
#if defined(LEXXOS_KERNEL_FREERTOS)
		return pvPortMalloc(size);
#else
		return std::malloc(size);
#endif
	}

	void freeRaw(void* memory) {
		if (memory == nullptr) {
			return;
		}
#if defined(LEXXOS_KERNEL_FREERTOS)
		vPortFree(memory);
#else
		std::free(memory);
#endif
	}

	std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment) {
		auto const mask = alignment - 1U;
		return (value + mask) & ~mask;
	}

} // namespace

std::string_view toString(ProgramMemoryArenaStatus status) {
	switch (status) {
		case ProgramMemoryArenaStatus::Ready:
			return "ready";
		case ProgramMemoryArenaStatus::ExternalUnavailable:
			return "external-unavailable";
		case ProgramMemoryArenaStatus::InternalUnavailable:
			return "internal-unavailable";
	}

	return "unknown";
}

ProgramMemoryArena::~ProgramMemoryArena() {
	release();
}

ProgramMemoryArenaStatus ProgramMemoryArena::reserve(ProgramRuntimeProfile const& runtime) {
	release();
	_snapshot = {
	    .memoryClass = runtime.memoryClass,
	    .requestedExternalBytes = runtime.externalMemoryQuotaBytes,
	    .reservedExternalBytes = 0,
	    .requestedInternalBytes = runtime.internalMemoryQuotaBytes,
	    .reservedInternalBytes = 0,
	    .status = ProgramMemoryArenaStatus::Ready,
	    .active = false,
	};

	if (runtime.internalMemoryQuotaBytes != 0) {
		_internalStorage = allocateRaw(runtime.internalMemoryQuotaBytes);
		if (_internalStorage == nullptr) {
			_snapshot.status = ProgramMemoryArenaStatus::InternalUnavailable;
			return _snapshot.status;
		}
		_internalStorageBytes = runtime.internalMemoryQuotaBytes;
		_snapshot.reservedInternalBytes = runtime.internalMemoryQuotaBytes;
		_snapshot.active = true;
	}

	if (runtime.memoryClass == ProgramMemoryClass::InternalOnly && runtime.externalMemoryQuotaBytes == 0) {
		return _snapshot.status;
	}

#if FEATURE_ENABLED(PSRAM)
	_snapshot.reservedExternalBytes = runtime.externalMemoryQuotaBytes;
	_snapshot.active = _snapshot.active || runtime.externalMemoryQuotaBytes != 0;
	return _snapshot.status;
#else
	if (runtime.memoryClass == ProgramMemoryClass::ExternalRequired) {
		_snapshot.status = ProgramMemoryArenaStatus::ExternalUnavailable;
		return _snapshot.status;
	}

	// Пока платформенный allocator PSRAM не подключён, preferred-запросы
	// разрешаются без внешней квоты и продолжают работать через internal SRAM.
	return _snapshot.status;
#endif
}

void ProgramMemoryArena::release() {
	freeDetachedInternalStorage(detachInternalStorage());
}

void* ProgramMemoryArena::detachInternalStorage() {
	auto* const storage = _internalStorage;
	_internalStorage = nullptr;
	_internalStorageBytes = 0;
	_internalUsedBytes = 0;
	_snapshot.reservedExternalBytes = 0;
	_snapshot.reservedInternalBytes = 0;
	_snapshot.active = false;
	return storage;
}

void ProgramMemoryArena::freeDetachedInternalStorage(void* memory) {
	freeRaw(memory);
}

void* ProgramMemoryArena::allocate(std::size_t size, std::size_t alignment) {
	if (size == 0 || _internalStorage == nullptr || alignment == 0) {
		return nullptr;
	}

	if ((alignment & (alignment - 1U)) != 0) {
		return nullptr;
	}

	auto const alignedOffset = alignUp(_internalUsedBytes, static_cast<std::uint32_t>(alignment));
	if (size > _internalStorageBytes || alignedOffset > _internalStorageBytes - size) {
		return nullptr;
	}

	auto* const base = static_cast<std::uint8_t*>(_internalStorage);
	_internalUsedBytes = static_cast<std::uint32_t>(alignedOffset + size);
	return base + alignedOffset;
}

ProgramMemoryArenaSnapshot ProgramMemoryArena::snapshot() const {
	return _snapshot;
}

bool ProgramMemoryArena::active() const {
	return _snapshot.active;
}

} // namespace lexxos::program
