// RK3506G2 / PicoCalc Lyra CPU0 privileged short-descriptor VMSA policy.
// Firmware/OP-TEE/U-Boot regions are left Normal-NC and never allocated by us.
// BOOTRAM is broken into 4 KiB pages with read-only executable code, RW/XN
// data, faulting slack, and a distinct non-cacheable DMA pool.
#include <lexxos/platform/rockchip/rk3506/mmu_managed.h>

#include <cstdint>
#include <cstring>

#include <lexxos/platform/rockchip/rk3506/fdt_memory_map.h>

namespace lexxos::platform::rk3506::memory {

namespace {

	constexpr std::uint32_t kL1Entries = 4096U;
	constexpr std::uint32_t kL2Entries = 256U;
	constexpr std::uint32_t kBootFirst = 0x020U;
	constexpr std::uint32_t kBootPast = 0x028U;
	constexpr std::uint32_t kDdrPast = 0x080U;
	constexpr std::uintptr_t kBootStart = 0x02000000U;
	constexpr std::uintptr_t kDmaStart = 0x02700000U;
	constexpr std::uintptr_t kDmaPast = 0x027F0000U;
	constexpr std::uintptr_t kMonitorStart = 0x027F0000U;
	constexpr std::uint32_t kPage = 4096U;
	constexpr std::uint32_t kPoolPages = 64U;
	constexpr std::uint32_t kSection = 1U << 20U;
	constexpr std::uint32_t kL1Section = 0x2U;
	constexpr std::uint32_t kL1Coarse = 0x1U;
	constexpr std::uint32_t kL1ApPrivRw = 1U << 10U; // AP=001.
	constexpr std::uint32_t kL1NormalNc = 1U << 12U; // TEX=001, C=B=0.
	constexpr std::uint32_t kL1Shareable = 1U << 16U;
	constexpr std::uint32_t kL1Xn = 1U << 4U;
	constexpr std::uint32_t kL2ApPrivRw = 1U << 4U;
	constexpr std::uint32_t kL2ApPrivRo = (1U << 9U) | kL2ApPrivRw;
	constexpr std::uint32_t kL2NormalNc = 1U << 6U; // TEX=001.
	constexpr std::uint32_t kL2NormalWbwa = kL2NormalNc | (1U << 3U) | (1U << 2U);
	constexpr std::uint32_t kL2Small = 1U << 1U;
	constexpr std::uint32_t kL2Xn = 1U; // Small-page descriptor bit 0.

	// The platform-owned BOOTRAM slack is the ONLY allocatable source for
	// regular physical pages until FDT reserved-memory is fully audited.
	// Never allocate the original U-Boot 0x02600000 boot buffer region.
	constexpr std::uintptr_t kOwnedPagesPast = 0x02600000U;
	constexpr std::uint32_t kOwnedSlots = static_cast<std::uint32_t>((kOwnedPagesPast - kBootStart) / kPage);
	constexpr std::uint32_t kMaximumOwnedAllocation = 64U; // bound IRQ-off page table updates.

#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	// Opt-in firmware-release hypothesis only: not a discoverable free-RAM bank.
	// Does not touch U-Boot's historical 0x03000000 image staging or FIT
	// / relocated U-Boot above 0x04000000 in the observed boot trace.
	constexpr std::uintptr_t kExtendedStart = 0x02800000U;
	constexpr std::uintptr_t kExtendedPast = 0x03000000U;
	constexpr std::uint32_t kExtendedFirstSection = kExtendedStart >> 20U;
	constexpr std::uint32_t kExtendedLastSection = kExtendedPast >> 20U;
	constexpr std::uint32_t kExtendedSlots = static_cast<std::uint32_t>((kExtendedPast - kExtendedStart) / kPage);
	constexpr std::uint32_t kExtendedMaxAllocation = 256U; // 1 MiB per reservation.
#endif

	// Tables and bitmap live in zeroed BOOTRAM .bss; no heap or C++ constructors.
	alignas(16384) std::uint32_t l1[kL1Entries]{};
	alignas(1024) std::uint32_t l2[kBootPast - kBootFirst][kL2Entries]{};
	std::uint8_t usedPages[kPoolPages]{};
	std::uint8_t allocationSpan[kPoolPages]{};

	// One slot per physical BOOTRAM page below the firmware scratch reserve.
	// Only pages after alignUp(__image_end__) can ever be allocated.
	std::uint8_t ownedUsed[kOwnedSlots]{};
	std::uint16_t ownedSpan[kOwnedSlots]{};

#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	alignas(1024) std::uint32_t extendedL2[kExtendedLastSection - kExtendedFirstSection][kL2Entries]{};
	std::uint8_t extendedUsed[kExtendedSlots]{};
	std::uint16_t extendedSpan[kExtendedSlots]{};
#endif

	// NOLOAD: never force the firmware image to include a 256 KiB zero block.
	// The linker explicitly separates this from the cacheable .bss and heap.
	alignas(kPage) std::uint8_t pool[kPoolPages * kPage]
	    __attribute__((section(".dma"), used));

	extern "C" char __rx_end__[];
	extern "C" char __image_end__[];
	extern "C" char __dma_start__[];
	extern "C" char __dma_end__[];

	std::uintptr_t rxEnd() {
		return reinterpret_cast<std::uintptr_t>(__rx_end__);
	}

	std::uintptr_t imageEnd() {
		return reinterpret_cast<std::uintptr_t>(__image_end__);
	}

	std::uint32_t dataLineBytes() {
		std::uint32_t ctr{};
		asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
		return 4U << ((ctr >> 16U) & 0xFU); // CTR.DminLine.
	}

	std::uint32_t writeBackGranule() {
		std::uint32_t ctr{};
		asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
		auto const cwg = (ctr >> 24U) & 0xFU;
		auto const line = dataLineBytes();
		if (cwg == 0U) {
			return line;
		}

		auto const granule = 4U << cwg;
		return granule > line ? granule : line;
	}

	bool rangeWithin(std::uintptr_t start, std::size_t bytes,
	    std::uintptr_t first, std::uintptr_t past) {
		return bytes != 0U && start >= first && start < past && bytes <= past - start;
	}

	std::uint32_t smallPage(std::uintptr_t physical, bool writable, bool executable, bool cached) {
		return static_cast<std::uint32_t>(physical) | kL2Small | (writable ? kL2ApPrivRw : kL2ApPrivRo) | (executable ? 0U : kL2Xn) | (cached ? kL2NormalWbwa : kL2NormalNc);
	}

	void buildTables() {
		for (std::uint32_t section = 0; section < kL1Entries; ++section) {
			auto const physical = section << 20U;
			auto const attributes = section < kDdrPast
			    ? kL1NormalNc | kL1Shareable | kL1Xn
			    : (1U << 2U) | kL1Xn; // Shareable Device, never executable.
			l1[section] = physical | kL1Section | kL1ApPrivRw | attributes;
		}

		for (std::uint32_t section = kBootFirst; section < kBootPast; ++section) {
			auto& table = l2[section - kBootFirst];
			l1[section] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table))
			    | kL1Coarse; // Domain 0, checks AP; TTBR0 short descriptors.
			for (std::uint32_t page = 0; page < kL2Entries; ++page) {
				auto const address = static_cast<std::uintptr_t>(section) * kSection
				    + static_cast<std::uintptr_t>(page) * kPage;
				if (address >= kMonitorStart) {
					table[page] = 0U; // GDB monitor reservation: not ours.
				} else if (address >= kDmaStart) {
					table[page] = smallPage(address, true, false, false);
				} else if (address < rxEnd()) {
					table[page] = smallPage(address, false, true, true);
				} else if (address < imageEnd()) {
					table[page] = smallPage(address, true, false, true);
				} else {
					table[page] = 0U; // Catch accesses beyond the linked image.
				}
			}
		}

#if defined(LEXXOS_RK3506_DDR_EXTENDED)
		// Every extended frame initially faults. A frame becomes WBWA RW/XN
		// only after allocation; no other RAM region changes ownership.
		for (std::uint32_t section = kExtendedFirstSection; section < kExtendedLastSection; ++section) {
			auto& table = extendedL2[section - kExtendedFirstSection];
			l1[section] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table)) | kL1Coarse;
			for (auto& entry : table) {
				entry = 0U;
			}
		}
#endif
	}

	std::uintptr_t firstOwnedPage() {
		// The 4 KiB region holding the last linker object is never reused.
		return (imageEnd() + kPage - 1U) & ~static_cast<std::uintptr_t>(kPage - 1U);
	}

	std::uint32_t firstOwnedSlot() {
		auto const first = firstOwnedPage();
		if (first < kBootStart || first >= kOwnedPagesPast) {
			return kOwnedSlots;
		}

		return static_cast<std::uint32_t>((first - kBootStart) / kPage);
	}

	// Caller must mask IRQ and own the frame. Update a single short-descriptor
	// PTE, clean the table entry to PoC, then invalidate CPU0 TLB by MVA.
	// All mappings are identity VA=PA, WBWA RW/XN, privileged-only.
	void changePageMapping(std::uintptr_t physical, bool present, std::uint32_t& descriptor) {
		if (!present) {
			// Dirty cache lines must not survive unmapping and later reuse.
			// No concurrent CPU or DMA owner is permitted during release.
			auto const line = dataLineBytes();
			asm volatile("dsb sy" ::: "memory");
			for (auto address = physical; address < physical + kPage; address += line) {
				auto const mva = static_cast<std::uint32_t>(address);
				asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(mva) : "memory"); // DCCIMVAC
			}

			asm volatile("dsb sy" ::: "memory");
		}

		descriptor = present ? smallPage(physical, true, false, true) : 0U;
		auto const entryMva = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&descriptor));
		auto const pageMva = static_cast<std::uint32_t>(physical);
		asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(entryMva) : "memory"); // DCCMVAC
		asm volatile("dsb sy" ::: "memory");
		asm volatile("mcr p15, 0, %0, c8, c7, 1" : : "r"(pageMva) : "memory"); // TLBIMVA, CPU0
		asm volatile("dsb sy; isb sy" ::: "memory");
	}

	void changeOwnedMapping(std::uintptr_t physical, bool present) {
		auto& descriptor = l2[(physical >> 20U) - kBootFirst][(physical >> 12U) & 0xFFU];
		changePageMapping(physical, present, descriptor);
	}

#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	void changeExtendedMapping(std::uintptr_t physical, bool present) {
		auto& descriptor = extendedL2[(physical >> 20U) - kExtendedFirstSection][(physical >> 12U) & 0xFFU];
		changePageMapping(physical, present, descriptor);
	}
#endif

	void invalidateColdBootDataLines() {
		auto const line = dataLineBytes();
		asm volatile("dsb sy" ::: "memory");
		for (std::uint32_t address = static_cast<std::uint32_t>(kBootStart);
		    address < kDmaStart; address += line) {
			asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(address) : "memory");
		}

#if defined(LEXXOS_RK3506_DDR_EXTENDED)
		// U-Boot must have cleaned and disabled D-cache before handoff.
		// Discard any non-dirty stale CPU0 lines before mapping the new pool.
		for (std::uint32_t address = kExtendedStart; address < kExtendedPast; address += line) {
			asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(address) : "memory");
		}
#endif

		asm volatile("dsb sy" ::: "memory");
	}

	std::uint32_t maskIrq() {
		std::uint32_t cpsr{};
		asm volatile("mrs %0, cpsr\n\tcpsid i" : "=r"(cpsr) : : "memory");
		return cpsr;
	}

	void restoreIrq(std::uint32_t cpsr) {
		asm volatile("msr cpsr_c, %0" : : "r"(cpsr) : "memory");
	}

	enum class CacheAction {
		Clean,
		Invalidate,
		CleanInvalidate
	};

	bool maintainCache(void const* pointer, std::size_t bytes, CacheAction action) {
		if (pointer == nullptr || bytes == 0U) {
			return false;
		}

		std::uint32_t sctlr{};
		asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
		if ((sctlr & 0x5U) != 0x5U) {
			return false;
		}

		auto const start = reinterpret_cast<std::uintptr_t>(pointer);
		if (rangeWithin(start, bytes, kDmaStart, kDmaPast)) {
			asm volatile("dsb sy" ::: "memory");
			return true; // The whole DMA pool is Normal non-cacheable.
		}

		if (!rangeWithin(start, bytes, kBootStart, imageEnd())) {
			return false;
		}

		auto const granule = writeBackGranule();
		if (action != CacheAction::Clean
		    && ((start | bytes) & (granule - 1U)) != 0U) {
			return false;
		}

		auto const line = dataLineBytes();
		auto address = start & ~static_cast<std::uintptr_t>(line - 1U);
		auto const end = start + bytes;
		asm volatile("dsb sy" ::: "memory");
		while (address < end) {
			auto const value = static_cast<std::uint32_t>(address);
			switch (action) {
				case CacheAction::Clean:
					asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r"(value) : "memory");
					break;
				case CacheAction::Invalidate:
					asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(value) : "memory");
					break;
				case CacheAction::CleanInvalidate:
					asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(value) : "memory");
					break;
			}

			address += line;
		}

		asm volatile("dsb sy" ::: "memory");
		return true;
	}

} // namespace

// Called once from startup.S, with IRQ/FIQ masked, before C++ constructors.
bool rk3506_mmu_managed_enable_internal(std::uint32_t sctlr) {
	buildTables();
	invalidateColdBootDataLines();
	std::uint32_t const zero = 0U;
	std::uint32_t const ttbr = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(l1));
	std::uint32_t const dacr = 0x1U; // Domain 0 client; enforce L1/L2 AP.
	asm volatile("dsb sy" ::: "memory");
	asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(zero) : "memory"); // TTBCR
	asm volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttbr) : "memory"); // TTBR0
	asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr) : "memory"); // DACR
	asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero) : "memory"); // TLBIALL
	asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero) : "memory"); // ICIALLU
	asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero) : "memory"); // BPIALL
	asm volatile("dsb sy; isb sy" ::: "memory");
	// Direct TEX encoding (TRE=0), legacy AP bits (AFE=0) and WXN=1:
	// no writable page can become executable even if a descriptor regresses.
	sctlr &= ~((1U << 2U) | (1U << 28U) | (1U << 29U));
	sctlr |= (1U << 19U) | 0x5U; // WXN, MMU and D-cache; I-cache inherited.
	asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
	asm volatile("isb sy" ::: "memory");
	return true;
}

ManagedMmuSnapshot snapshot() {
	std::uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
	auto const cpsr = maskIrq();
	std::size_t freePages = 0;
	for (auto slot : usedPages) {
		if (slot == 0) {
			++freePages;
		}
	}

	restoreIrq(cpsr);
	return {
	    .executableEnd = rxEnd(),
	    .imageEnd = imageEnd(),
	    .dmaStart = reinterpret_cast<std::uintptr_t>(__dma_start__),
	    .dmaEnd = reinterpret_cast<std::uintptr_t>(__dma_end__),
	    .dmaPoolBytes = sizeof(pool),
	    .dmaFreeBytes = freePages * kPage,
	    .cacheLineBytes = dataLineBytes(),
	    .active = (sctlr & 0x5U) == 0x5U,
	};
}

void* allocateDma(std::size_t bytes) {
	if (bytes == 0U || bytes > sizeof(pool)) {
		return nullptr;
	}

	auto const pages = static_cast<std::uint32_t>((bytes + kPage - 1U) / kPage);
	auto const cpsr = maskIrq();
	for (std::uint32_t first = 0; first + pages <= kPoolPages; ++first) {
		bool available = true;
		for (std::uint32_t i = 0; i < pages; ++i) {
			if (usedPages[first + i] != 0U) {
				available = false;
				break;
			}
		}

		if (!available) {
			continue;
		}

		for (std::uint32_t i = 0; i < pages; ++i) {
			usedPages[first + i] = 1U;
		}

		allocationSpan[first] = static_cast<std::uint8_t>(pages);
		restoreIrq(cpsr);
		return pool + first * kPage;
	}

	restoreIrq(cpsr);
	return nullptr;
}

bool freeDma(void* pointer) {
	if (pointer == nullptr) {
		return false;
	}

	auto const address = reinterpret_cast<std::uintptr_t>(pointer);
	auto const base = reinterpret_cast<std::uintptr_t>(pool);
	if (!rangeWithin(address, 1U, base, base + sizeof(pool))) {
		return false;
	}

	auto const offset = address - base;
	if ((offset & (kPage - 1U)) != 0U) {
		return false;
	}

	auto const first = static_cast<std::uint32_t>(offset / kPage);
	auto const cpsr = maskIrq();
	auto const pages = allocationSpan[first];
	if (pages == 0U || first + pages > kPoolPages) {
		restoreIrq(cpsr);
		return false;
	}

	for (std::uint32_t i = 0; i < pages; ++i) {
		usedPages[first + i] = 0U;
	}

	allocationSpan[first] = 0U;
	restoreIrq(cpsr);
	return true;
}

bool isDmaBuffer(void const* pointer, std::size_t bytes) {
	if (pointer == nullptr) {
		return false;
	}

	auto const start = reinterpret_cast<std::uintptr_t>(pointer);
	auto const base = reinterpret_cast<std::uintptr_t>(pool);
	if (!rangeWithin(start, bytes, base, base + sizeof(pool))) {
		return false;
	}

	auto const index = static_cast<std::uint32_t>((start - base) / kPage);
	auto const cpsr = maskIrq();
	bool valid = false;
	for (std::uint32_t first = 0; first <= index; ++first) {
		auto const count = allocationSpan[first];
		if (count != 0U && first + count > index) {
			auto const end = base + static_cast<std::uintptr_t>(first + count) * kPage;
			valid = bytes <= end - start;
			break;
		}
	}

	restoreIrq(cpsr);
	return valid;
}

OwnedPagesSnapshot ownedPagesSnapshot() {
	auto const first = firstOwnedSlot();
	auto const cpsr = maskIrq();
	std::uint32_t free = 0U;
	std::uint32_t largest = 0U;
	std::uint32_t run = 0U;
	std::uint32_t allocations = 0U;
	for (std::uint32_t index = first; index < kOwnedSlots; ++index) {
		if (ownedUsed[index] == 0U) {
			++free;
			if (++run > largest) {
				largest = run;
			}
		} else {
			run = 0U;
		}

		if (ownedSpan[index] != 0U) {
			++allocations;
		}
	}

	restoreIrq(cpsr);
	return {
	    .first = first < kOwnedSlots ? kBootStart + static_cast<std::uintptr_t>(first) * kPage : kOwnedPagesPast,
	    .past = kOwnedPagesPast,
	    .totalPages = kOwnedSlots - first,
	    .freePages = free,
	    .largestFreeRun = largest,
	    .activeAllocations = allocations,
	};
}

void* allocateOwnedPages(std::size_t pages) {
	if (pages == 0U || pages > kMaximumOwnedAllocation) {
		return nullptr;
	}

	std::uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
	if ((sctlr & 0x5U) != 0x5U) {
		return nullptr; // Only the active managed MMU owns this pool.
	}

	auto const first = firstOwnedSlot();
	if (pages > kOwnedSlots - first) {
		return nullptr;
	}

	auto const cpsr = maskIrq();
	for (std::uint32_t start = first; start + pages <= kOwnedSlots; ++start) {
		bool vacant = true;
		for (std::size_t i = 0; i < pages; ++i) {
			if (ownedUsed[start + i]) {
				vacant = false;
				break;
			}
		}

		if (!vacant) {
			continue;
		}

		for (std::size_t i = 0; i < pages; ++i) {
			ownedUsed[start + i] = 1U;
		}

		ownedSpan[start] = static_cast<std::uint16_t>(pages);
		auto const address = kBootStart + static_cast<std::uintptr_t>(start) * kPage;
		for (std::size_t i = 0; i < pages; ++i) {
			changeOwnedMapping(address + i * kPage, true);
		}

		restoreIrq(cpsr);
		// Do not keep interrupts disabled for the potentially large zeroing.
		// The reserved frames are private until this function returns.
		std::memset(reinterpret_cast<void*>(address), 0, pages * kPage);
		return reinterpret_cast<void*>(address);
	}

	restoreIrq(cpsr);
	return nullptr;
}

bool freeOwnedPages(void* pointer) {
	if (pointer == nullptr) {
		return false;
	}

	auto const address = reinterpret_cast<std::uintptr_t>(pointer);
	auto const first = firstOwnedSlot();
	if (address < kBootStart + static_cast<std::uintptr_t>(first) * kPage
	    || address >= kOwnedPagesPast
	    || (address & (kPage - 1U)) != 0U) {
		return false;
	}

	auto const index = static_cast<std::uint32_t>((address - kBootStart) / kPage);
	auto const cpsr = maskIrq();
	auto const count = ownedSpan[index];
	if (count == 0U || index + count > kOwnedSlots) {
		restoreIrq(cpsr);
		return false;
	}

	for (std::uint32_t i = 0; i < count; ++i) {
		changeOwnedMapping(address + i * kPage, false);
		ownedUsed[index + i] = 0U;
	}

	ownedSpan[index] = 0U;
	restoreIrq(cpsr);
	return true;
}

bool ownedPagesSelfTest() {
	auto const before = ownedPagesSnapshot();
	auto* const bytes = static_cast<std::uint8_t*>(allocateOwnedPages(2U));
	if (!bytes) {
		return false;
	}

	std::uint8_t volatile* const data = bytes;
	for (std::size_t i = 0; i < 2U * kPage; ++i) {
		data[i] = static_cast<std::uint8_t>((i * 37U + 19U) & 0xFFU);
	}

	bool success = true;
	for (std::size_t i = 0; i < 2U * kPage; ++i) {
		if (data[i] != static_cast<std::uint8_t>((i * 37U + 19U) & 0xFFU)) {
			success = false;
			break;
		}
	}

	if (!freeOwnedPages(bytes)) {
		return false;
	}

	auto const after = ownedPagesSnapshot();
	return success && before.freePages == after.freePages && before.activeAllocations == after.activeAllocations;
}

ExtendedPagesSnapshot extendedPagesSnapshot() {
#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	auto const cpsr = maskIrq();
	std::uint32_t free = 0U;
	std::uint32_t largest = 0U;
	std::uint32_t run = 0U;
	std::uint32_t allocations = 0U;
	for (std::uint32_t index = 0; index < kExtendedSlots; ++index) {
		if (extendedUsed[index] == 0U) {
			++free;
			if (++run > largest) {
				largest = run;
			}
		} else {
			run = 0U;
		}

		if (extendedSpan[index] != 0U) {
			++allocations;
		}
	}

	restoreIrq(cpsr);

	return {.first = kExtendedStart, .past = kExtendedPast, .totalPages = kExtendedSlots, .freePages = free, .largestFreeRun = largest, .activeAllocations = allocations, .enabled = fdtRangeUnreserved(kExtendedStart, kExtendedPast - kExtendedStart)};

#else
	return {};
#endif
}

bool isExtendedAddress(void const* pointer) {
#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	auto const address = reinterpret_cast<std::uintptr_t>(pointer);
	return address >= kExtendedStart && address < kExtendedPast;
#else
	(void) pointer;
	return false;
#endif
}

void* allocateExtendedPages(std::size_t pages) {
#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	// Device tree banks AND every static reservation must permit this whole
	// region. A missing/corrupt DTB fails closed without affecting FreeRTOS.
	if (pages == 0U || pages > kExtendedMaxAllocation || !fdtRangeUnreserved(kExtendedStart, kExtendedPast - kExtendedStart)) {
		return nullptr;
	}

	std::uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
	if ((sctlr & 0x5U) != 0x5U) {
		return nullptr;
	}

	// CPU0 only: the bitmap, PTE updates and TLB changes are serialized.
	auto const cpsr = maskIrq();
	for (std::uint32_t start = 0U; start + pages <= kExtendedSlots; ++start) {
		bool vacant = true;
		for (std::size_t i = 0; i < pages; ++i) {
			if (extendedUsed[start + i] != 0U) {
				vacant = false;
				break;
			}
		}

		if (!vacant) {
			continue;
		}

		for (std::size_t i = 0; i < pages; ++i) {
			extendedUsed[start + i] = 1U;
		}

		extendedSpan[start] = static_cast<std::uint16_t>(pages);
		auto const address = kExtendedStart + static_cast<std::uintptr_t>(start) * kPage;
		for (std::size_t i = 0; i < pages; ++i) {
			changeExtendedMapping(address + i * kPage, true);
		}

		restoreIrq(cpsr);
		// Caller cannot observe this range before it is zero-initialized.
		std::memset(reinterpret_cast<void*>(address), 0, pages * kPage);
		return reinterpret_cast<void*>(address);
	}

	restoreIrq(cpsr);
#else
	(void) pages;
#endif
	return nullptr;
}

bool freeExtendedPages(void* pointer) {
#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	if (!isExtendedAddress(pointer)) {
		return false;
	}

	auto const address = reinterpret_cast<std::uintptr_t>(pointer);
	if ((address & (kPage - 1U)) != 0U) {
		return false;
	}

	auto const index = static_cast<std::uint32_t>((address - kExtendedStart) / kPage);
	auto const cpsr = maskIrq();
	auto const count = extendedSpan[index];
	if (count == 0U || index + count > kExtendedSlots) {
		restoreIrq(cpsr);
		return false;
	}

	for (std::uint32_t i = 0; i < count; ++i) {
		changeExtendedMapping(address + i * kPage, false);
		extendedUsed[index + i] = 0U;
	}

	extendedSpan[index] = 0U;
	restoreIrq(cpsr);
	return true;
#else
	(void) pointer;
	return false;
#endif
}

bool extendedPagesSelfTest() {
#if defined(LEXXOS_RK3506_DDR_EXTENDED)
	auto const before = extendedPagesSnapshot();
	auto* const bytes = static_cast<std::uint8_t*>(allocateExtendedPages(2U));
	if (bytes == nullptr) {
		return false;
	}

	std::uint8_t volatile* const data = bytes;
	bool success = true;
	for (std::size_t index = 0; index < 2U * kPage; ++index) {
		data[index] = static_cast<std::uint8_t>((index * 53U + 7U) & 0xFFU);
	}

	for (std::size_t index = 0; index < 2U * kPage; ++index) {
		if (data[index] != static_cast<std::uint8_t>((index * 53U + 7U) & 0xFFU)) {
			success = false;
			break;
		}
	}

	if (!freeExtendedPages(bytes)) {
		return false;
	}

	auto const after = extendedPagesSnapshot();
	return success && before.freePages == after.freePages && before.activeAllocations == after.activeAllocations;
#else
	return false;
#endif
}

bool cleanForDevice(void const* pointer, std::size_t bytes) {
	return maintainCache(pointer, bytes, CacheAction::Clean);
}

bool invalidateFromDevice(void* pointer, std::size_t bytes) {
	return maintainCache(pointer, bytes, CacheAction::Invalidate);
}

bool cleanInvalidate(void* pointer, std::size_t bytes) {
	return maintainCache(pointer, bytes, CacheAction::CleanInvalidate);
}

} // namespace lexxos::platform::rk3506::memory

extern "C" bool rk3506_mmu_identity_normal_nc_initialize() {
	using lexxos::platform::rk3506::memory::snapshot;
	// Boot handoff: U-Boot must leave MMU/D-cache disabled and dirty lines clean.
	std::uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
	if ((sctlr & 0x5U) != 0U) {
		return false;
	}

	// These symbols describe the linked BOOTRAM image and separate NOLOAD pool.
	auto const info = snapshot();
	if (info.executableEnd < 0x02000000U || info.executableEnd >= info.imageEnd || info.imageEnd > 0x02700000U || (info.executableEnd & 4095U) != 0U || info.dmaStart != 0x02700000U || info.dmaEnd > 0x027F0000U || info.dmaPoolBytes > info.dmaEnd - info.dmaStart) {
		return false;
	}

	// Page tables are initialized before any cacheable mapping is enabled.
	// The platform owns no memory outside its BOOTRAM and DMA reservation.
	// Defined below in the internal namespace to keep the API narrow.
	return lexxos::platform::rk3506::memory::rk3506_mmu_managed_enable_internal(sctlr);
}
