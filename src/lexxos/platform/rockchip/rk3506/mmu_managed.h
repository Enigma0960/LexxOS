#pragma once

#include <cstddef>
#include <cstdint>

// CPU0, privileged mode only. Identity-mapped physical pointers are valid DMA
// bus addresses ONLY inside the explicitly owned non-cacheable pool.
namespace lexxos::platform::rk3506::memory {

struct ManagedMmuSnapshot {
	std::uintptr_t executableEnd{};
	std::uintptr_t imageEnd{};
	std::uintptr_t dmaStart{};
	std::uintptr_t dmaEnd{};
	std::size_t dmaPoolBytes{};
	std::size_t dmaFreeBytes{};
	std::uint32_t cacheLineBytes{};
	bool active{};
};

[[nodiscard]] ManagedMmuSnapshot snapshot();

struct OwnedPagesSnapshot {
	std::uintptr_t first{};
	std::uintptr_t past{};
	std::uint32_t totalPages{};
	std::uint32_t freePages{};
	std::uint32_t largestFreeRun{};
	std::uint32_t activeAllocations{};
};

// Initially faulting BOOTRAM pages, mapped on demand to privileged RW/XN WBWA.
// Explicitly excludes firmware boot scratch and all non-BOOTRAM DDR. No DMA.
// Maximum one allocation: 64 physically contiguous 4 KiB pages (256 KiB).
// Caller must release only after all users have stopped; never from ISR/FIQ.
[[nodiscard]] OwnedPagesSnapshot ownedPagesSnapshot();
[[nodiscard]] void* allocateOwnedPages(std::size_t pages);
[[nodiscard]] bool freeOwnedPages(void* pointer);
[[nodiscard]] bool ownedPagesSelfTest();

struct ExtendedPagesSnapshot {
	std::uintptr_t first{};
	std::uintptr_t past{};
	std::uint32_t totalPages{};
	std::uint32_t freePages{};
	std::uint32_t largestFreeRun{};
	std::uint32_t activeAllocations{};
	bool enabled{};
};

// Optional 8 MiB application DDR: 4 KiB RW/XN WBWA pages, unmapped when free
// Only valid when LEXXOS_RK3506_DDR_EXTENDED=ON. Not coherent with DMA.
// Address ownership is board/firmware-specific; never use from an IRQ.
[[nodiscard]] ExtendedPagesSnapshot extendedPagesSnapshot();
[[nodiscard]] void* allocateExtendedPages(std::size_t pages);
[[nodiscard]] bool freeExtendedPages(void* pointer);
[[nodiscard]] bool isExtendedAddress(void const* pointer);
[[nodiscard]] bool extendedPagesSelfTest();

// 4 KiB page-granular buffers, physically contiguous, non-cacheable, CPU0.
// These functions cannot be called from IRQ/FIQ handlers. Memory returned by
// allocateDma is uninitialized and must be fully initialized before use.
[[nodiscard]] void* allocateDma(std::size_t bytes);
[[nodiscard]] bool freeDma(void* pointer);
[[nodiscard]] bool isDmaBuffer(void const* pointer, std::size_t bytes);

// Explicit PoC cache operations. Fail closed on unowned/invalid addresses.
// Invalidation requires whole cache-line granules to avoid losing adjacent
// dirty CPU data. Do not call while DMA is accessing the same buffer.
[[nodiscard]] bool cleanForDevice(void const* pointer, std::size_t bytes);
[[nodiscard]] bool invalidateFromDevice(void* pointer, std::size_t bytes);
[[nodiscard]] bool cleanInvalidate(void* pointer, std::size_t bytes);

} // namespace lexxos::platform::rk3506::memory
