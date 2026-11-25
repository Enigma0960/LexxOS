#include <lexxos/platform/rockchip/rk3506/fdt_memory_map.h>

#include <cstdint>

// Cortex-A7 receives DTB pointer in r2 when entered by U-Boot bootm. Do not
// use a fixed DTB address: U-Boot may relocate it between image versions.
extern "C" {
std::uint32_t rk3506_boot_fdt_address;
}

namespace lexxos::platform::rk3506::memory {

namespace {

	constexpr std::uint32_t kLexxStart = 0x02000000U;
	constexpr std::uint32_t kMaximumFdtBytes = 1024U * 1024U;
	constexpr std::uint32_t kHeaderBytes = 40U;
	constexpr std::uint32_t kBeginNode = 1U;
	constexpr std::uint32_t kEndNode = 2U;
	constexpr std::uint32_t kProperty = 3U;
	constexpr std::uint32_t kNoop = 4U;
	constexpr std::uint32_t kEnd = 9U;

	FdtMemoryMap g_map{};
	bool g_captured{};

	std::uint32_t be32(std::uint8_t const* bytes) {
		return (std::uint32_t(bytes[0]) << 24U) | (std::uint32_t(bytes[1]) << 16U) | (std::uint32_t(bytes[2]) << 8U) | bytes[3];
	}

	std::uint64_t be64(std::uint8_t const* bytes) {
		return (std::uint64_t(be32(bytes)) << 32U) | be32(bytes + 4U);
	}

	bool contains(std::uint32_t offset, std::uint32_t size, std::uint32_t total) {
		return offset <= total && size <= total - offset;
	}

	bool nameEquals(std::uint8_t const* name, std::uint32_t available, char const* value) {
		std::uint32_t i = 0;
		while (value[i] != '\0') {
			if (i >= available || name[i] != static_cast<std::uint8_t>(value[i])) {
				return false;
			}

			++i;
		}

		return i < available && name[i] == 0U;
	}

	bool nodeIs(std::uint8_t const* name, std::uint32_t available, char const* value) {
		std::uint32_t i = 0;
		while (value[i] != '\0') {
			if (i >= available || name[i] != static_cast<std::uint8_t>(value[i])) {
				return false;
			}

			++i;
		}

		return i < available && (name[i] == 0U || name[i] == '@');
	}

	bool append(FdtPhysicalRange* output, std::uint32_t& count, std::uint32_t capacity,
	    std::uint64_t first, std::uint64_t bytes) {
		if (bytes == 0U) {
			return true;
		}

		if (count >= capacity || first > UINT64_MAX - bytes) {
			return false;
		}

		output[count++] = {.first = first, .past = first + bytes};
		return true;
	}

	bool decodeReg(std::uint8_t const* data, std::uint32_t bytes,
	    std::uint32_t addressCells, std::uint32_t sizeCells, bool reservation) {
		if (addressCells < 1U || addressCells > 2U || sizeCells < 1U || sizeCells > 2U) {
			return false;
		}

		auto const stride = (addressCells + sizeCells) * 4U;
		if (bytes % stride != 0U) {
			return false;
		}

		for (std::uint32_t offset = 0; offset < bytes; offset += stride) {
			auto address = std::uint64_t(be32(data + offset));
			if (addressCells == 2U) {
				address = (address << 32U) | be32(data + offset + 4U);
			}

			auto const sizeOffset = offset + addressCells * 4U;
			auto size = std::uint64_t(be32(data + sizeOffset));
			if (sizeCells == 2U) {
				size = (size << 32U) | be32(data + sizeOffset + 4U);
			}

			if (reservation) {
				if (!append(g_map.reserved, g_map.reservedCount,
				        FdtMemoryMap::maximumReservations, address, size)) {
					return false;
				}
			} else if (!append(g_map.banks, g_map.bankCount,
			               FdtMemoryMap::maximumBanks, address, size)) {
				return false;
			}
		}

		return true;
	}

	// Two passes: first collect root/parent cell widths (independent of node
	// ordering); then decode reg arrays with the correct inherited widths.
	bool walk(std::uint8_t const* blob, std::uint32_t structureOffset,
	    std::uint32_t structureBytes, std::uint32_t stringsOffset,
	    std::uint32_t stringsBytes, bool cellsOnly,
	    std::uint32_t& rootAddress, std::uint32_t& rootSize,
	    std::uint32_t& reservedAddress, std::uint32_t& reservedSize) {
		auto cursor = structureOffset;
		auto const end = structureOffset + structureBytes;
		std::uint32_t depth = 0U;
		bool memoryNode = false;
		bool reservedParent = false;
		bool reservedChild = false;
		bool ended = false;
		while (cursor < end) {
			if (!contains(cursor, 4U, end)) {
				return false;
			}

			auto const token = be32(blob + cursor);
			cursor += 4U;

			if (token == kBeginNode) {
				auto const start = cursor;
				while (cursor < end && blob[cursor] != 0U) {
					++cursor;
				}

				if (cursor == end || depth >= 32U) {
					return false;
				}

				auto const nameBytes = cursor - start + 1U;
				++cursor;
				if (cursor > UINT32_MAX - 3U) {
					return false;
				}

				cursor = (cursor + 3U) & ~3U;
				if (cursor > end) {
					return false;
				}

				++depth;
				if (depth == 2U) {
					memoryNode = nodeIs(blob + start, nameBytes, "memory");
					reservedParent = nodeIs(blob + start, nameBytes, "reserved-memory");
				} else if (depth == 3U) {
					reservedChild = reservedParent;
				}
			} else if (token == kEndNode) {
				if (depth == 0U) {
					return false;
				}

				if (depth == 3U) {
					reservedChild = false;
				}

				if (depth == 2U) {
					memoryNode = false;
					reservedParent = false;
				}

				--depth;
			} else if (token == kProperty) {
				if (!contains(cursor, 8U, end)) {
					return false;
				}

				auto const size = be32(blob + cursor);
				auto const nameOffset = be32(blob + cursor + 4U);
				cursor += 8U;
				if (!contains(cursor, size, end) || nameOffset >= stringsBytes) {
					return false;
				}

				auto const* name = blob + stringsOffset + nameOffset;
				auto const nameBytes = stringsBytes - nameOffset;
				bool terminated = false;
				for (std::uint32_t i = 0; i < nameBytes; ++i) {
					if (name[i] == 0U) {
						terminated = true;
						break;
					}
				}

				if (!terminated) {
					return false;
				}

				auto const* value = blob + cursor;
				if (cellsOnly) {
					if (size == 4U) {
						if (depth == 1U && nameEquals(name, nameBytes, "#address-cells")) {
							rootAddress = be32(value);
						}

						if (depth == 1U && nameEquals(name, nameBytes, "#size-cells")) {
							rootSize = be32(value);
						}

						if (depth == 2U && reservedParent && nameEquals(name, nameBytes, "#address-cells")) {
							reservedAddress = be32(value);
						}

						if (depth == 2U && reservedParent && nameEquals(name, nameBytes, "#size-cells")) {
							reservedSize = be32(value);
						}
					}
				} else {
					if (depth == 2U && memoryNode && nameEquals(name, nameBytes, "reg") && !decodeReg(value, size, rootAddress, rootSize, false)) {
						return false;
					}

					if (depth == 3U && reservedChild && nameEquals(name, nameBytes, "reg") && !decodeReg(value, size, reservedAddress, reservedSize, true)) {
						return false;
					}

					if (depth == 2U && reservedParent && nameEquals(name, nameBytes, "ranges")) {
						if (size != 0U) {
							return false; // no address translation support
						}

						g_map.reservationsIdentityMapped = true;
					}
				}

				if (size > UINT32_MAX - 3U) {
					return false;
				}

				cursor = (cursor + size + 3U) & ~3U;
				if (cursor > end) {
					return false;
				}
			} else if (token == kNoop) {
				continue;
			} else if (token == kEnd) {
				if (depth != 0U) {
					return false;
				}

				ended = true;
				break;
			} else {
				return false;
			}
		}

		return ended;
	}

	void capture() {
		g_captured = true;
		g_map = {};
		g_map.sourceAddress = rk3506_boot_fdt_address;
		g_map.status = "invalid-boot-argument";
		auto const source = g_map.sourceAddress;
		if (source < 4096U || source > kLexxStart - kHeaderBytes) {
			return;
		}

		auto const* blob = reinterpret_cast<std::uint8_t const*>(static_cast<std::uintptr_t>(source));
		if (be32(blob) != 0xD00DFEEDU) {
			g_map.status = "bad-magic";
			return;
		}

		auto const total = be32(blob + 4U);
		g_map.sourceBytes = total;
		if (total < kHeaderBytes || total > kMaximumFdtBytes || total > kLexxStart - source) {
			g_map.status = "invalid-size";
			return;
		}

		auto const structureOffset = be32(blob + 8U);
		auto const stringsOffset = be32(blob + 12U);
		auto const reserveOffset = be32(blob + 16U);
		auto const version = be32(blob + 20U);
		auto const lastCompatible = be32(blob + 24U);
		auto const stringsBytes = be32(blob + 32U);
		auto const structureBytes = be32(blob + 36U);

		if (version < 17U || lastCompatible > 17U || !contains(structureOffset, structureBytes, total) || !contains(stringsOffset, stringsBytes, total) || !contains(reserveOffset, 16U, total)) {
			g_map.status = "invalid-header";
			return;
		}

		// FDT reserve map: big-endian uint64_t address/size pairs, 0/0 terminator.
		auto pos = reserveOffset;
		bool terminated = false;
		while (contains(pos, 16U, total)) {
			auto const first = be64(blob + pos);
			auto const bytes = be64(blob + pos + 8U);
			pos += 16U;
			if (first == 0U && bytes == 0U) {
				terminated = true;
				break;
			}

			if (!append(g_map.reserved, g_map.reservedCount,
			        FdtMemoryMap::maximumReservations, first, bytes)) {
				break;
			}
		}

		if (!terminated) {
			g_map.status = "invalid-memreserve";
			return;
		}

		std::uint32_t rootAddress = 2U;
		std::uint32_t rootSize = 1U;
		std::uint32_t reservedAddress = 0U;
		std::uint32_t reservedSize = 0U;

		if (!walk(blob, structureOffset, structureBytes, stringsOffset, stringsBytes,
		        true, rootAddress, rootSize, reservedAddress, reservedSize)) {
			g_map.status = "invalid-tree";
			return;
		}

		if (reservedAddress == 0U) {
			reservedAddress = rootAddress;
		}

		if (reservedSize == 0U) {
			reservedSize = rootSize;
		}

		if (rootAddress < 1U || rootAddress > 2U || rootSize < 1U || rootSize > 2U || reservedAddress < 1U || reservedAddress > 2U || reservedSize < 1U || reservedSize > 2U || !walk(blob, structureOffset, structureBytes, stringsOffset, stringsBytes, false, rootAddress, rootSize, reservedAddress, reservedSize) || g_map.bankCount == 0U) {
			g_map.status = "invalid-regions";
			return;
		}

		g_map.status = "ok";
		g_map.valid = true;
	}

} // namespace

extern "C" void rk3506_fdt_capture() {
	capture();
}

FdtMemoryMap const& firmwareMemoryMap() {
	// Normally captured before the scheduler starts. A GDB/debug entry that
	// missed the early call is deliberately not parsed from untrusted memory.
	return g_map;
}

bool fdtRangeUnreserved(std::uintptr_t first, std::size_t bytes) {
	if (!g_captured || !g_map.valid || !g_map.reservationsIdentityMapped || bytes == 0U) {
		return false;
	}

	auto const start = std::uint64_t(first);
	if (start > UINT64_MAX - bytes) {
		return false;
	}

	auto const past = start + bytes;
	bool inRam = false;
	for (std::uint32_t i = 0; i < g_map.bankCount; ++i) {
		if (start >= g_map.banks[i].first && past <= g_map.banks[i].past) {
			inRam = true;
			break;
		}
	}

	if (!inRam) {
		return false;
	}

	for (std::uint32_t i = 0; i < g_map.reservedCount; ++i) {
		if (start < g_map.reserved[i].past && g_map.reserved[i].first < past) {
			return false;
		}
	}

	return true;
}

} // namespace lexxos::platform::rk3506::memory
