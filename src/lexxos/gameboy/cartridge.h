#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/gameboy/cartridge_header.h>
#include <lexxos/gameboy/cartridge_types.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::gameboy {

[[nodiscard]] CartridgeMapper mapperFromCartridgeType(std::uint8_t type);
[[nodiscard]] std::string_view mapperName(CartridgeMapper mapper);
[[nodiscard]] CartridgeFeatures cartridgeFeatures(std::uint8_t type);
[[nodiscard]] std::uint32_t romBankCount(std::uint8_t romSizeCode);
[[nodiscard]] std::uint32_t ramBankCount(std::uint8_t ramSizeCode);

class Cartridge final {
public:
	static constexpr std::uint32_t romBankSize = 16U * 1024U;
	static constexpr std::uint32_t ramBankSize = 8U * 1024U;
#if defined(LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES)
	static constexpr std::size_t cacheSize = static_cast<std::size_t>(LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES);
#else
	static constexpr std::size_t cacheSize = 4U * 1024U;
#endif
#if defined(LEXXOS_GAMEBOY_ROM_CACHE_BANKS)
	static constexpr std::size_t cacheSlotCount =
	    static_cast<std::size_t>(LEXXOS_GAMEBOY_ROM_CACHE_BANKS) * (romBankSize / cacheSize);
#elif defined(LEXXOS_PLATFORM_PICO)
	static constexpr std::size_t cacheSlotCount = 8U;
#else
	static constexpr std::size_t cacheSlotCount = 16U;
#endif
	static_assert(cacheSize >= 512U, "Game Boy ROM cache page must be at least one SD sector");
	static_assert((cacheSize & (cacheSize - 1U)) == 0U, "Game Boy ROM cache page must be a power of two");
	static_assert((romBankSize % cacheSize) == 0U, "Game Boy ROM cache page must divide a ROM bank");
	static_assert(cacheSlotCount >= 1U, "Game Boy ROM cache must keep at least one page");
	static_assert(cacheSlotCount <= 16U, "Game Boy ROM cache is intentionally capped to protect embedded RAM");
	static constexpr std::size_t externalRamCapacity = 32U * 1024U;

	Cartridge();
	explicit Cartridge(lexxos::platform::PlatformServices const& platform);
	~Cartridge();
	Cartridge(Cartridge const&) = delete;
	Cartridge& operator=(Cartridge const&) = delete;

	void bindPlatformServices(lexxos::platform::PlatformServices const& platform);
	[[nodiscard]] CartridgeOpenResult open(std::string_view path);
	[[nodiscard]] CartridgeFlashLoadResult loadRomToPlatformFlash();
	[[nodiscard]] CartridgeFlashLoadResult reusePlatformFlashRomIfMatching();
	[[nodiscard]] CartridgePsramLoadResult loadRomToPlatformPsram();
	[[nodiscard]] CartridgePsramLoadResult reusePlatformPsramRomIfMatching();
	void close();

	[[nodiscard]] bool isOpen() const;
	[[nodiscard]] std::string_view path() const;
	[[nodiscard]] std::uint64_t fileSize() const;
	[[nodiscard]] CartridgeHeaderInfo const& headerInfo() const;
	[[nodiscard]] CartridgeMapper mapper() const;
	[[nodiscard]] CartridgeFeatures features() const;
	[[nodiscard]] bool mapperSupported() const;
	[[nodiscard]] std::uint32_t expectedRomSize() const;
	[[nodiscard]] std::uint32_t expectedRomBanks() const;
	[[nodiscard]] std::uint32_t expectedRamSize() const;
	[[nodiscard]] std::uint32_t expectedRamBanks() const;
	[[nodiscard]] std::uint16_t selectedRomBank() const;
	[[nodiscard]] std::uint8_t selectedRamBank() const;
	[[nodiscard]] bool ramEnabled() const;
	[[nodiscard]] std::uint32_t externalRamReadCount() const;
	[[nodiscard]] std::uint32_t externalRamWriteCount() const;

	[[nodiscard]] lexxos::vfs::ReadStatus read8(std::uint16_t address, std::uint8_t& value);
	[[nodiscard]] std::uint8_t read8Fast(std::uint16_t address);
	void write8(std::uint16_t address, std::uint8_t value);
	[[nodiscard]] lexxos::vfs::ReadStatus readExternalRam(std::uint16_t address, std::uint8_t& value);
	[[nodiscard]] std::uint8_t readExternalRamFast(std::uint16_t address);
	void writeExternalRam(std::uint16_t address, std::uint8_t value);
	void resetMapperState();

	[[nodiscard]] std::uint32_t cacheFillCount() const;
	[[nodiscard]] std::uint32_t cacheRetryCount() const;
	[[nodiscard]] std::uint32_t cacheFailureCount() const;
	[[nodiscard]] std::uint64_t cacheReadBytes() const;
	[[nodiscard]] std::uint32_t cacheFillMs() const;
	[[nodiscard]] bool usingPlatformFlashRom() const;
	[[nodiscard]] std::uint32_t platformFlashRomBytes() const;
	[[nodiscard]] bool usingPlatformPsramRom() const;
	[[nodiscard]] std::uint32_t platformPsramRomBytes() const;
	[[nodiscard]] std::uint8_t const* fixedRomData() const;
	[[nodiscard]] std::uint8_t const* selectedRomData() const;

	[[nodiscard]] static constexpr std::uint32_t cachePageBytes() { return static_cast<std::uint32_t>(cacheSize); }

	[[nodiscard]] static constexpr std::uint32_t cacheSlots() { return static_cast<std::uint32_t>(cacheSlotCount); }

	[[nodiscard]] static constexpr std::uint32_t cacheCapacityBytes() {
		return static_cast<std::uint32_t>(cacheSize * cacheSlotCount);
	}

private:
#if defined(LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE)
	struct CacheSlot {
		std::uint8_t* data{nullptr};
		std::uint32_t base{0};
		std::size_t bytes{0};
		std::uint32_t age{0};
		bool valid{false};
	};
#else
	using CacheSlot = detail::CartridgeCacheSlot<cacheSize>;
#endif

	[[nodiscard]] bool ensureCacheSlots();
	[[nodiscard]] CacheSlot* cacheSlotsBegin();
	[[nodiscard]] CacheSlot* cacheSlotsEnd();
	void releaseCacheSlots();
	[[nodiscard]] lexxos::vfs::ReadStatus readRomOffset(std::uint32_t offset, std::uint8_t& value);
	[[nodiscard]] std::uint8_t readRomOffsetFast(std::uint32_t offset);
	[[nodiscard]] lexxos::vfs::ReadStatus fillCacheSlot(CacheSlot& slot, std::uint32_t cacheBase, std::size_t minimumBytes);
	[[nodiscard]] std::uint32_t mapRomAddress(std::uint16_t address) const;
	[[nodiscard]] std::uint32_t normalizedSwitchableRomBank(std::uint32_t bank) const;
	void refreshSelectedRomBankCache();
	[[nodiscard]] std::size_t mappedExternalRamOffset(std::uint16_t address) const;
	[[nodiscard]] std::size_t activeExternalRamSize() const;
	[[nodiscard]] lexxos::platform::PlatformServices const& platformServices() const;
	[[nodiscard]] std::uint32_t uptimeMs() const;
	void sleepMs(std::uint32_t milliseconds) const;
	void invalidateCache();

	lexxos::platform::PlatformServices const* _platformServices{nullptr};
	std::uint8_t const* _platformFlashRom{nullptr};
	std::uint32_t _platformFlashRomBytes{0};
	bool _platformPsramRom{false};
	std::uint32_t _platformPsramRomBytes{0};
	lexxos::vfs::FileHandle _file;
	std::array<char, lexxos::vfs::maxPathLength> _path{};
	std::size_t _pathSize{0};
	std::uint64_t _fileSize{0};
	std::array<std::uint8_t, cartridgeHeaderSize> _headerRaw{};
	CartridgeHeaderInfo _header{};
	CartridgeMapper _mapper{CartridgeMapper::Unsupported};
	CartridgeFeatures _features{};
	bool _open{false};

	std::uint8_t _mbc1RomBankLow5{1};
	std::uint8_t _mbc1BankHigh2{0};
	std::uint8_t _mbc1Mode{0};
	std::uint8_t _mbc2RomBank{1};
	std::uint16_t _mbc5RomBank{1};
	std::uint8_t _ramBank{0};
	bool _ramEnabled{false};
	std::array<std::uint8_t, externalRamCapacity> _externalRam{};
	std::uint32_t _externalRamReadCount{0};
	std::uint32_t _externalRamWriteCount{0};
	std::uint16_t _selectedRomBank{1};
	std::uint32_t _selectedRomOffsetBase{romBankSize};

	std::array<CacheSlot, cacheSlotCount> _cacheSlots{};
	CacheSlot* _hotCacheSlot{nullptr};
	CacheSlot* _previousHotCacheSlot{nullptr};
	std::uint32_t _cacheAge{0};
	std::uint32_t _cacheFillCount{0};
	std::uint32_t _cacheRetryCount{0};
	std::uint32_t _cacheFailureCount{0};
	std::uint64_t _cacheReadBytes{0};
	std::uint32_t _cacheFillMs{0};
};

} // namespace lexxos::gameboy
