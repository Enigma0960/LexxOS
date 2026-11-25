#pragma once

#include <cstddef>
#include <cstdint>

// RK3506 CPU0, ARM Linux boot protocol: r2 points to the flattened device tree.
// This is diagnostic firmware evidence, NOT permission to allocate arbitrary DDR.
namespace lexxos::platform::rk3506::memory {

struct FdtPhysicalRange {
	std::uint64_t first{};
	std::uint64_t past{};
};

struct FdtMemoryMap {
	static constexpr std::size_t maximumBanks = 8U;
	static constexpr std::size_t maximumReservations = 96U;

	FdtPhysicalRange banks[maximumBanks]{};
	FdtPhysicalRange reserved[maximumReservations]{};
	std::uint32_t sourceAddress{};
	std::uint32_t sourceBytes{};
	std::uint32_t bankCount{};
	std::uint32_t reservedCount{};
	char const* status{"not-captured"};
	bool valid{};
	bool reservationsIdentityMapped{};
};

// Runs before the MMU is enabled, after .bss has been cleared. Never aborts
// boot if the FDT is absent (e.g. a GDB RAM entry without boot protocol args).
extern "C" void rk3506_fdt_capture();
[[nodiscard]] FdtMemoryMap const& firmwareMemoryMap();

// Fail closed: range must be covered by a RAM bank and disjoint from every
// FDT /memreserve/ and /reserved-memory/* static reg reservation.
// Known U-Boot runtime staging areas still require additional board policy.
[[nodiscard]] bool fdtRangeUnreserved(std::uintptr_t first, std::size_t bytes);

} // namespace lexxos::platform::rk3506::memory
