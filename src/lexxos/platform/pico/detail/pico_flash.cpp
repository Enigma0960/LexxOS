#include <lexxos/platform/platform.h>

#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/sync.h>

namespace {

#ifndef PICO_FLASH_SIZE_BYTES
// Keep this as a conservative fallback for custom board definitions. Pico SDK
// board profiles normally provide the real value.
constexpr std::uint32_t picoFlashSizeBytes = 2U * 1024U * 1024U;
#else
constexpr std::uint32_t picoFlashSizeBytes = PICO_FLASH_SIZE_BYTES;
#endif

#ifndef LEXXOS_GAMEBOY_FLASH_ROM_OFFSET
constexpr std::uint32_t gameBoyFlashRomOffset = 1024U * 1024U;
#else
constexpr std::uint32_t gameBoyFlashRomOffset = LEXXOS_GAMEBOY_FLASH_ROM_OFFSET;
#endif

constexpr std::uint32_t flashSafetyMargin = 64U * 1024U;

extern "C" char __flash_binary_end;

constexpr std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment) {
	auto const mask = alignment - 1U;
	return (value + mask) & ~mask;
}

std::uint32_t runningBinaryEndOffset() {
	auto const end = reinterpret_cast<std::uintptr_t>(&__flash_binary_end);
	if (end < XIP_BASE) {
		return 0;
	}

	auto const offset = end - static_cast<std::uintptr_t>(XIP_BASE);
	if (offset > picoFlashSizeBytes || offset > UINT32_MAX) {
		return 0;
	}
	return static_cast<std::uint32_t>(offset);
}

bool stagingRegionOverlapsFirmware(std::uint32_t offset) {
	auto const firmwareEnd = runningBinaryEndOffset();
	if (firmwareEnd == 0) {
		return false;
	}
	if (firmwareEnd > UINT32_MAX - flashSafetyMargin) {
		return true;
	}
	auto const protectedEnd = alignUp(firmwareEnd + flashSafetyMargin, FLASH_SECTOR_SIZE);
	return offset < protectedEnd;
}

} // namespace

bool lexxos::platform::gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = {};
	if ((gameBoyFlashRomOffset % FLASH_SECTOR_SIZE) != 0U
	    || gameBoyFlashRomOffset >= picoFlashSizeBytes
	    || stagingRegionOverlapsFirmware(gameBoyFlashRomOffset)) {
		return false;
	}

	output.available = true;
	output.offset = gameBoyFlashRomOffset;
	output.capacity = picoFlashSizeBytes - gameBoyFlashRomOffset;
	output.eraseSize = FLASH_SECTOR_SIZE;
	output.programSize = FLASH_PAGE_SIZE;
	output.xipBase = reinterpret_cast<std::uint8_t const*>(XIP_BASE + gameBoyFlashRomOffset);
	return true;
}

bool lexxos::platform::eraseAndProgramGameBoyRomFlash(std::uint32_t offset, void const* data, std::size_t size) {
	GameBoyRomFlashRegion region{};
	if (!gameBoyRomFlashRegion(region) || data == nullptr || size == 0) {
		return false;
	}

	if (size > UINT32_MAX
	    || size > region.capacity
	    || offset < region.offset
	    || offset - region.offset > region.capacity - static_cast<std::uint32_t>(size)) {
		return false;
	}

	if ((offset % region.eraseSize) != 0U || (size % region.eraseSize) != 0U) {
		return false;
	}

	// Flash XIP недоступен во время записи: весь erase/program этап выполняем с запрещёнными IRQ.
	auto const interrupts = save_and_disable_interrupts();
	flash_range_erase(offset, size);
	for (std::size_t programmed = 0; programmed < size; programmed += region.programSize) {
		flash_range_program(offset + static_cast<std::uint32_t>(programmed),
		    static_cast<std::uint8_t const*>(data) + programmed,
		    region.programSize);
	}
	restore_interrupts(interrupts);
	return true;
}
