// Experimental ARMv7-A CPU0-only identity map for PicoCalc / Lyra M.
// Maps DDR as Normal-NC and MMIO as Device/XN. With the explicit BOOTRAM
// flag only the linked 8 MiB LexxOS region becomes Normal WBWA, non-shareable.
// Never use DMA buffers in that region without a future coherent allocator.
#include <stdint.h>

namespace {

constexpr uint32_t sectionCount = 4096U; // 4 GiB / 1 MiB.
constexpr uint32_t sectionShift = 20U;
constexpr uint32_t lyraDdrSections = 128U; // 128 MiB DDR, physical 0.
constexpr uint32_t sectionDescriptor = 0x2U;
constexpr uint32_t apFullAccess = 0x3U << 10U;
constexpr uint32_t normalNonCacheable = 0x1U << 12U; // TEX=001,C=0,B=0
#if defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
constexpr uint32_t normalWriteBackAllocate = (1U << 12U) | (1U << 3U) | (1U << 2U); // TEX=001,C=1,B=1
constexpr uint32_t bootRamFirstSection = 0x020U;
constexpr uint32_t bootRamPastEndSection = 0x028U;
#endif
constexpr uint32_t shareable = 0x1U << 16U; // S=1
constexpr uint32_t executeNever = 0x1U << 4U; // Section XN

// Short-descriptor first-level table: 16 KiB aligned, 16 KiB in .bss.
// Do not use the heap: this runs before C++ global constructors.
alignas(16384) uint32_t l1Sections[sectionCount]{};

#if defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
// Invalidate only LexxOS BOOTRAM by MVA while caches are still disabled.
// Do not invalidate all sets/ways: secure firmware and other masters may own
// lines elsewhere in the shared cluster cache. U-Boot's cache-off handoff is
// a required precondition; no dirty data may be left in this region.
void invalidateBootRamDataCache() {
	uint32_t ctr{};
	asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ctr));
	auto const lineBytes = 4U << ((ctr >> 16U) & 0xFU);
	asm volatile("dsb sy" ::: "memory");
	for (uint32_t address = bootRamFirstSection << sectionShift;
	    address < bootRamPastEndSection << sectionShift;
	    address += lineBytes) {
		asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(address) : "memory"); // DCIMVAC
	}

	asm volatile("dsb sy; isb sy" ::: "memory");
}
#endif

} // namespace

// Return false rather than trying to reprogram translation while a different
// firmware-owned MMU layout or a dirty D-cache might still be active.
extern "C" bool rk3506_mmu_identity_normal_nc_initialize() {
	uint32_t sctlr{};
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
	if ((sctlr & 0x5U) != 0U) { // SCTLR.M or SCTLR.C
		return false;
	}

	for (uint32_t section = 0; section < sectionCount; ++section) {
		uint32_t const address = section << sectionShift;
		uint32_t attributes = section < lyraDdrSections
		    ? (normalNonCacheable | shareable)
		    : ((1U << 2U) | executeNever); // Device, TEX=000,C=0,B=1
#if defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
		if (section >= bootRamFirstSection && section < bootRamPastEndSection) {
			attributes = normalWriteBackAllocate; // CPU0-only, non-shareable.
		}
#endif
		l1Sections[section] = address | sectionDescriptor | apFullAccess | attributes;
	}

	uint32_t const zero = 0U;
	uint32_t const ttbr = static_cast<uint32_t>(
	    reinterpret_cast<uintptr_t>(l1Sections));
	uint32_t const dacr = 0x1U; // Domain 0 is Client (checks AP).
	asm volatile("dsb sy" ::: "memory");

	// Short-descriptor translation, TTBR0 covers all 4 GiB (TTBCR.N=0).
	asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(zero) : "memory"); // TTBCR
	asm volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttbr) : "memory"); // TTBR0
	asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr) : "memory"); // DACR
	asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero) : "memory"); // TLBIALL
	asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero) : "memory"); // ICIALLU
	asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero) : "memory"); // BPIALL
	asm volatile("dsb sy; isb sy" ::: "memory");

	// Do not inherit firmware TEX remap / access-flag / write-XN policy:
	// the descriptors above use direct TEX/C/B decoding, legacy AP bits,
	// and executable DDR sections (the initial identity map is still RWX).
	sctlr &= ~((1U << 2U) | (1U << 19U) | (1U << 28U) | (1U << 29U));
#if defined(LEXXOS_RK3506_DCACHE_BOOTRAM)
	invalidateBootRamDataCache();
	sctlr |= 0x5U; // SCTLR.M=1, SCTLR.C=1.
#else
	sctlr |= 1U; // SCTLR.M=1, SCTLR.C=0.
#endif
	asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
	asm volatile("isb sy" ::: "memory");

	return true;
}
