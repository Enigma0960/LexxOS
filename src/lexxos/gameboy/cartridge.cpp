#include <lexxos/gameboy/cartridge.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include <lexxos/platform/platform_services.h>

namespace lexxos::gameboy {
namespace {

	constexpr std::uint8_t romReadAttempts = 4;
	constexpr std::uint32_t romReadRetryDelayMs = 1;
	constexpr std::string_view gameBoyRomCacheRegionName{"gameboy-rom-cache"};
	constexpr std::string_view gameBoyRomPsramRegionName{"gameboy-rom-psram"};
	constexpr std::size_t gameBoyRomFlashSectorBufferBytes = 4096;

	bool copyPath(std::string_view source, std::array<char, lexxos::vfs::maxPathLength>& target, std::size_t& size) {
		if (source.size() + 1 > target.size()) {
			return false;
		}
		std::memcpy(target.data(), source.data(), source.size());
		target[source.size()] = '\0';
		size = source.size();
		return true;
	}

	bool typeIsOneOf(std::uint8_t type, std::initializer_list<std::uint8_t> values) {
		for (auto const value : values) {
			if (value == type) {
				return true;
			}
		}
		return false;
	}

	std::array<std::uint8_t, gameBoyRomFlashSectorBufferBytes>& gameBoyRomFlashSectorBuffer() {
		static std::array<std::uint8_t, gameBoyRomFlashSectorBufferBytes> sector{};
		return sector;
	}

	std::array<std::uint8_t, gameBoyRomFlashSectorBufferBytes>& gameBoyRomPsramVerifyBuffer() {
		static std::array<std::uint8_t, gameBoyRomFlashSectorBufferBytes> sector{};
		return sector;
	}

	bool verifyPsramRomAgainstFile(
	    lexxos::platform::PlatformExternalMemoryRegionService const& externalMemory,
	    lexxos::vfs::FileHandle& file,
	    std::uint64_t fileSize,
	    std::size_t transferBytes,
	    CartridgePsramLoadResult& result) {
		if (externalMemory.read == nullptr) {
			result.platformUnavailable = true;
			return false;
		}
		if (!file.seek(0)) {
			result.seekFailed = true;
			return false;
		}

		auto& expected = gameBoyRomFlashSectorBuffer();
		auto& actual = gameBoyRomPsramVerifyBuffer();
		std::uint32_t verified = 0;
		while (verified < fileSize) {
			auto const remainingFile = fileSize - verified;
			auto const remaining = static_cast<std::size_t>(std::min<std::uint64_t>(remainingFile, transferBytes));
			std::size_t bytesRead = 0;
			result.readStatus = file.read(expected.data(), remaining, bytesRead);
			if (result.readStatus != lexxos::vfs::ReadStatus::Ok) {
				return false;
			}
			if (bytesRead != remaining) {
				result.shortRead = true;
				return false;
			}
			if (!externalMemory.read(gameBoyRomPsramRegionName, verified, actual.data(), bytesRead)) {
				result.readFailed = true;
				return false;
			}
			if (std::memcmp(expected.data(), actual.data(), bytesRead) != 0) {
				for (std::size_t offset = 0; offset < bytesRead; ++offset) {
					if (expected[offset] != actual[offset]) {
						result.verifyFailed = true;
						result.verifyFailedOffset = verified + static_cast<std::uint32_t>(offset);
						result.verifyExpected = expected[offset];
						result.verifyActual = actual[offset];
						return false;
					}
				}
			}
			verified += static_cast<std::uint32_t>(bytesRead);
		}
		return true;
	}

} // namespace

Cartridge::Cartridge()
    : Cartridge(lexxos::platform::services()) {
}

Cartridge::Cartridge(lexxos::platform::PlatformServices const& platform)
    : _platformServices(&platform) {
}

Cartridge::~Cartridge() {
	releaseCacheSlots();
}

CartridgeMapper mapperFromCartridgeType(std::uint8_t type) {
	switch (type) {
		case 0x00:
		case 0x08:
		case 0x09:
			return CartridgeMapper::RomOnly;
		case 0x01:
		case 0x02:
		case 0x03:
			return CartridgeMapper::Mbc1;
		case 0x05:
		case 0x06:
			return CartridgeMapper::Mbc2;
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
			return CartridgeMapper::Mbc5;
	}
	return CartridgeMapper::Unsupported;
}

std::string_view mapperName(CartridgeMapper mapper) {
	switch (mapper) {
		case CartridgeMapper::RomOnly:
			return "ROM ONLY";
		case CartridgeMapper::Mbc1:
			return "MBC1";
		case CartridgeMapper::Mbc2:
			return "MBC2";
		case CartridgeMapper::Mbc5:
			return "MBC5";
		case CartridgeMapper::Unsupported:
			return "unsupported";
	}
	return "unsupported";
}

CartridgeFeatures cartridgeFeatures(std::uint8_t type) {
	CartridgeFeatures features{};
	features.externalRam = typeIsOneOf(type, {0x02, 0x03, 0x05, 0x06, 0x08, 0x09, 0x10, 0x12, 0x13, 0x1A, 0x1B, 0x1D, 0x1E, 0x22, 0xFF});
	features.battery = typeIsOneOf(type, {0x03, 0x06, 0x09, 0x0D, 0x0F, 0x10, 0x13, 0x1B, 0x1E, 0x22, 0xFF});
	features.timer = typeIsOneOf(type, {0x0F, 0x10});
	features.rumble = typeIsOneOf(type, {0x1C, 0x1D, 0x1E, 0x22});
	return features;
}

std::uint32_t romBankCount(std::uint8_t romSizeCode) {
	auto const size = romSizeBytes(romSizeCode);
	return size == 0 ? 0 : size / Cartridge::romBankSize;
}

std::uint32_t ramBankCount(std::uint8_t ramSizeCode) {
	auto const size = ramSizeBytes(ramSizeCode);
	return size == 0 ? 0 : std::max<std::uint32_t>(1, size / Cartridge::ramBankSize);
}

void Cartridge::bindPlatformServices(lexxos::platform::PlatformServices const& platform) {
	_platformServices = &platform;
}

bool Cartridge::ensureCacheSlots() {
#if defined(LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE)
	for (auto& slot : _cacheSlots) {
		if (slot.data != nullptr) {
			continue;
		}
		slot.data = static_cast<std::uint8_t*>(std::malloc(cacheSize));
		if (slot.data == nullptr) {
			releaseCacheSlots();
			return false;
		}
	}
#endif
	return true;
}

Cartridge::CacheSlot* Cartridge::cacheSlotsBegin() {
	return _cacheSlots.data();
}

Cartridge::CacheSlot* Cartridge::cacheSlotsEnd() {
	auto* const begin = cacheSlotsBegin();
	return begin == nullptr ? nullptr : begin + cacheSlotCount;
}

void Cartridge::releaseCacheSlots() {
#if defined(LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE)
	for (auto& slot : _cacheSlots) {
		std::free(slot.data);
		slot = CacheSlot{};
	}
#endif
	_hotCacheSlot = nullptr;
	_previousHotCacheSlot = nullptr;
}

CartridgeOpenResult Cartridge::open(std::string_view path) {
	close();
	CartridgeOpenResult result{};
	if (!copyPath(path, _path, _pathSize)) {
		result.openStatus = lexxos::vfs::OpenStatus::InvalidPath;
		return result;
	}

	result.openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, _file);
	if (result.openStatus != lexxos::vfs::OpenStatus::Ok) {
		return result;
	}

	_fileSize = _file.size();
	if (!hasCompleteCartridgeHeader(static_cast<std::size_t>(_fileSize))) {
		result.tooSmall = true;
		_file.close();
		return result;
	}

	std::size_t bytesRead = 0;
	result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
	for (std::uint8_t attempt = 0; attempt < romReadAttempts; ++attempt) {
		bytesRead = 0;
		if (!_file.seek(0)) {
			result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
		} else {
			result.readStatus = _file.read(_headerRaw.data(), _headerRaw.size(), bytesRead);
			if (result.readStatus == lexxos::vfs::ReadStatus::Ok && bytesRead == _headerRaw.size()) {
				break;
			}
		}
		sleepMs(romReadRetryDelayMs);
	}
	if (result.readStatus != lexxos::vfs::ReadStatus::Ok || bytesRead != _headerRaw.size()) {
		_file.close();
		return result;
	}

	_header = parseCartridgeHeader(_headerRaw.data(), _headerRaw.size());
	if (!_header.nintendoLogoOk || !_header.headerChecksumOk) {
		result.invalidHeader = true;
		_file.close();
		return result;
	}

	_mapper = mapperFromCartridgeType(_header.cartridgeType);
	_features = cartridgeFeatures(_header.cartridgeType);
	_open = true;
	resetMapperState();
	_externalRam.fill(0xFF);
	_externalRamReadCount = 0;
	_externalRamWriteCount = 0;
	invalidateCache();
	result.ok = true;
	return result;
}

CartridgeFlashLoadResult Cartridge::loadRomToPlatformFlash() {
	CartridgeFlashLoadResult result{};
	if (!_open || !_file.isOpen()) {
		result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
		return result;
	}

	lexxos::platform::GameBoyRomFlashRegion region{};
	auto const& flashRegions = platformServices().flashRegions;
	if (flashRegions.findRegion == nullptr || !flashRegions.findRegion(gameBoyRomCacheRegionName, region) || !region.available || region.xipBase == nullptr) {
		result.platformUnavailable = true;
		return result;
	}

	result.flashOffset = region.offset;
	result.flashCapacity = region.capacity;
	if (region.eraseSize == 0 || region.programSize == 0 || (region.eraseSize % region.programSize) != 0) {
		result.programFailed = true;
		return result;
	}
	auto const roundedSize = ((_fileSize + region.eraseSize - 1U) / region.eraseSize) * region.eraseSize;
	if (_fileSize == 0 || roundedSize > region.capacity || roundedSize > UINT32_MAX) {
		result.tooLarge = true;
		return result;
	}

	if (!_file.seek(0)) {
		result.seekFailed = true;
		return result;
	}

	auto& sector = gameBoyRomFlashSectorBuffer();
	if (sector.size() < region.eraseSize) {
		result.programFailed = true;
		return result;
	}

	std::uint32_t loaded = 0;
	while (loaded < roundedSize) {
		sector.fill(0xFF);
		auto const remainingFile = loaded < _fileSize ? _fileSize - loaded : 0U;
		auto const remaining = static_cast<std::size_t>(std::min<std::uint64_t>(remainingFile, region.eraseSize));
		std::size_t bytesRead = 0;
		result.readStatus = remaining == 0 ? lexxos::vfs::ReadStatus::Ok : _file.read(sector.data(), remaining, bytesRead);
		if (result.readStatus != lexxos::vfs::ReadStatus::Ok) {
			return result;
		}
		if (bytesRead != remaining) {
			result.shortRead = true;
			return result;
		}

		auto const targetOffset = region.offset + loaded;
		if (flashRegions.eraseAndProgram == nullptr || !flashRegions.eraseAndProgram(targetOffset, sector.data(), region.eraseSize)) {
			result.programFailed = true;
			return result;
		}
		loaded += static_cast<std::uint32_t>(region.eraseSize);
	}

	_platformFlashRom = region.xipBase;
	_platformFlashRomBytes = static_cast<std::uint32_t>(_fileSize);
	_file.close();
	invalidateCache();
	result.ok = true;
	result.bytesLoaded = static_cast<std::uint32_t>(_fileSize);
	return result;
}

CartridgeFlashLoadResult Cartridge::reusePlatformFlashRomIfMatching() {
	CartridgeFlashLoadResult result{};
	if (!_open) {
		result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
		return result;
	}

	lexxos::platform::GameBoyRomFlashRegion region{};
	auto const& flashRegions = platformServices().flashRegions;
	if (flashRegions.findRegion == nullptr || !flashRegions.findRegion(gameBoyRomCacheRegionName, region) || !region.available || region.xipBase == nullptr) {
		result.platformUnavailable = true;
		return result;
	}

	result.flashOffset = region.offset;
	result.flashCapacity = region.capacity;
	if (_fileSize == 0 || _fileSize > region.capacity || _fileSize > UINT32_MAX) {
		result.tooLarge = true;
		return result;
	}

	auto const* flash = region.xipBase;
	auto const flashHeader = parseCartridgeHeader(flash, cartridgeHeaderSize);
	if (!flashHeader.nintendoLogoOk || !flashHeader.headerChecksumOk) {
		result.noMatchingImage = true;
		return result;
	}
	if (romSizeBytes(flashHeader.romSizeCode) != static_cast<std::uint32_t>(_fileSize)) {
		result.noMatchingImage = true;
		return result;
	}
	if (std::memcmp(flash, _headerRaw.data(), _headerRaw.size()) != 0) {
		result.noMatchingImage = true;
		return result;
	}

	_platformFlashRom = flash;
	_platformFlashRomBytes = static_cast<std::uint32_t>(_fileSize);
	_file.close();
	invalidateCache();
	result.ok = true;
	result.bytesLoaded = static_cast<std::uint32_t>(_fileSize);
	return result;
}

CartridgePsramLoadResult Cartridge::loadRomToPlatformPsram() {
	CartridgePsramLoadResult result{};
	if (!_open || !_file.isOpen()) {
		result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
		return result;
	}

	lexxos::platform::GameBoyRomExternalMemoryRegion region{};
	auto const& externalMemory = platformServices().externalMemory;
	if (externalMemory.findRegion == nullptr
	    || externalMemory.write == nullptr
	    || externalMemory.read == nullptr
	    || !externalMemory.findRegion(gameBoyRomPsramRegionName, region)
	    || !region.available) {
		result.platformUnavailable = true;
		return result;
	}

	result.capacity = region.capacity;
	result.transferBytes = region.preferredTransferBytes;
	if (_fileSize == 0 || _fileSize > region.capacity || _fileSize > UINT32_MAX) {
		result.tooLarge = true;
		return result;
	}

	if (!_file.seek(0)) {
		result.seekFailed = true;
		return result;
	}

	auto& buffer = gameBoyRomFlashSectorBuffer();
	std::size_t transferBytes = region.preferredTransferBytes == 0 ? buffer.size() : region.preferredTransferBytes;
	transferBytes = std::max<std::size_t>(1, std::min<std::size_t>(buffer.size(), transferBytes));
	result.transferBytes = static_cast<std::uint32_t>(transferBytes);

	std::uint32_t loaded = 0;
	while (loaded < _fileSize) {
		auto const remainingFile = _fileSize - loaded;
		auto const remaining = static_cast<std::size_t>(std::min<std::uint64_t>(remainingFile, transferBytes));
		std::size_t bytesRead = 0;
		result.readStatus = _file.read(buffer.data(), remaining, bytesRead);
		if (result.readStatus != lexxos::vfs::ReadStatus::Ok) {
			return result;
		}
		if (bytesRead != remaining) {
			result.shortRead = true;
			return result;
		}
		if (!externalMemory.write(gameBoyRomPsramRegionName, loaded, buffer.data(), bytesRead)) {
			result.writeFailed = true;
			return result;
		}
		loaded += static_cast<std::uint32_t>(bytesRead);
	}

	if (!verifyPsramRomAgainstFile(externalMemory, _file, _fileSize, transferBytes, result)) {
		return result;
	}

	_platformFlashRom = nullptr;
	_platformFlashRomBytes = 0;
	_platformPsramRom = true;
	_platformPsramRomBytes = static_cast<std::uint32_t>(_fileSize);
	_file.close();
	invalidateCache();
	result.ok = true;
	result.bytesLoaded = static_cast<std::uint32_t>(_fileSize);
	return result;
}

CartridgePsramLoadResult Cartridge::reusePlatformPsramRomIfMatching() {
	CartridgePsramLoadResult result{};
	if (!_open) {
		result.readStatus = lexxos::vfs::ReadStatus::NotReadable;
		return result;
	}

	lexxos::platform::GameBoyRomExternalMemoryRegion region{};
	auto const& externalMemory = platformServices().externalMemory;
	if (externalMemory.findRegion == nullptr
	    || externalMemory.read == nullptr
	    || !externalMemory.findRegion(gameBoyRomPsramRegionName, region)
	    || !region.available) {
		result.platformUnavailable = true;
		return result;
	}

	result.capacity = region.capacity;
	result.transferBytes = region.preferredTransferBytes;
	if (_fileSize == 0 || _fileSize > region.capacity || _fileSize > UINT32_MAX) {
		result.tooLarge = true;
		return result;
	}

	std::array<std::uint8_t, cartridgeHeaderSize> psramHeader{};
	if (!externalMemory.read(gameBoyRomPsramRegionName, 0, psramHeader.data(), psramHeader.size())) {
		result.readFailed = true;
		return result;
	}

	auto const parsedHeader = parseCartridgeHeader(psramHeader.data(), psramHeader.size());
	if (!parsedHeader.nintendoLogoOk || !parsedHeader.headerChecksumOk) {
		result.noMatchingImage = true;
		return result;
	}
	if (romSizeBytes(parsedHeader.romSizeCode) != static_cast<std::uint32_t>(_fileSize)) {
		result.noMatchingImage = true;
		return result;
	}
	if (std::memcmp(psramHeader.data(), _headerRaw.data(), _headerRaw.size()) != 0) {
		result.noMatchingImage = true;
		return result;
	}

	std::size_t transferBytes = region.preferredTransferBytes == 0 ? gameBoyRomFlashSectorBuffer().size() : region.preferredTransferBytes;
	transferBytes = std::max<std::size_t>(1, std::min<std::size_t>(gameBoyRomFlashSectorBuffer().size(), transferBytes));
	result.transferBytes = static_cast<std::uint32_t>(transferBytes);
	if (!verifyPsramRomAgainstFile(externalMemory, _file, _fileSize, transferBytes, result)) {
		return result;
	}

	_platformFlashRom = nullptr;
	_platformFlashRomBytes = 0;
	_platformPsramRom = true;
	_platformPsramRomBytes = static_cast<std::uint32_t>(_fileSize);
	_file.close();
	invalidateCache();
	result.ok = true;
	result.bytesLoaded = static_cast<std::uint32_t>(_fileSize);
	return result;
}

void Cartridge::close() {
	_file.close();
	_path = {};
	_pathSize = 0;
	_fileSize = 0;
	_headerRaw = {};
	_header = {};
	_mapper = CartridgeMapper::Unsupported;
	_features = {};
	_open = false;
	_platformFlashRom = nullptr;
	_platformFlashRomBytes = 0;
	_platformPsramRom = false;
	_platformPsramRomBytes = 0;
	resetMapperState();
	_externalRam.fill(0xFF);
	_externalRamReadCount = 0;
	_externalRamWriteCount = 0;
	invalidateCache();
	releaseCacheSlots();
}

bool Cartridge::isOpen() const {
	return _open;
}

std::string_view Cartridge::path() const {
	return std::string_view{_path.data(), _pathSize};
}

std::uint64_t Cartridge::fileSize() const {
	return _fileSize;
}

CartridgeHeaderInfo const& Cartridge::headerInfo() const {
	return _header;
}

CartridgeMapper Cartridge::mapper() const {
	return _mapper;
}

CartridgeFeatures Cartridge::features() const {
	return _features;
}

bool Cartridge::mapperSupported() const {
	return _mapper != CartridgeMapper::Unsupported;
}

std::uint32_t Cartridge::expectedRomSize() const {
	return romSizeBytes(_header.romSizeCode);
}

std::uint32_t Cartridge::expectedRomBanks() const {
	return romBankCount(_header.romSizeCode);
}

std::uint32_t Cartridge::expectedRamSize() const {
	if (_mapper == CartridgeMapper::Mbc2) {
		return 512U;
	}
	return ramSizeBytes(_header.ramSizeCode);
}

std::uint32_t Cartridge::expectedRamBanks() const {
	if (_mapper == CartridgeMapper::Mbc2) {
		return 1;
	}
	return ramBankCount(_header.ramSizeCode);
}

std::uint16_t Cartridge::selectedRomBank() const {
	return _selectedRomBank;
}

void Cartridge::refreshSelectedRomBankCache() {
	std::uint32_t bank = 0;
	if (_mapper == CartridgeMapper::Mbc5) {
		bank = _mbc5RomBank;
	} else if (_mapper == CartridgeMapper::Mbc2) {
		bank = static_cast<std::uint32_t>(_mbc2RomBank & 0x0FU);
		if (bank == 0) {
			bank = 1;
		}
	} else if (_mapper == CartridgeMapper::Mbc1) {
		bank = static_cast<std::uint32_t>(_mbc1RomBankLow5 & 0x1FU);
		if (bank == 0) {
			bank = 1;
		}
		bank |= static_cast<std::uint32_t>(_mbc1BankHigh2 & 0x03U) << 5U;
	} else {
		bank = expectedRomBanks() > 1 ? 1 : 0;
	}
	_selectedRomBank = static_cast<std::uint16_t>(normalizedSwitchableRomBank(bank));
	_selectedRomOffsetBase = static_cast<std::uint32_t>(_selectedRomBank) * romBankSize;
}

std::uint8_t Cartridge::selectedRamBank() const {
	return _ramBank;
}

bool Cartridge::ramEnabled() const {
	return _ramEnabled;
}

std::uint32_t Cartridge::externalRamReadCount() const {
	return _externalRamReadCount;
}

std::uint32_t Cartridge::externalRamWriteCount() const {
	return _externalRamWriteCount;
}

std::uint32_t Cartridge::cacheFillCount() const {
	return _cacheFillCount;
}

std::uint32_t Cartridge::cacheRetryCount() const {
	return _cacheRetryCount;
}

std::uint32_t Cartridge::cacheFailureCount() const {
	return _cacheFailureCount;
}

std::uint64_t Cartridge::cacheReadBytes() const {
	return _cacheReadBytes;
}

std::uint32_t Cartridge::cacheFillMs() const {
	return _cacheFillMs;
}

bool Cartridge::usingPlatformFlashRom() const {
	return _platformFlashRom != nullptr;
}

std::uint32_t Cartridge::platformFlashRomBytes() const {
	return _platformFlashRomBytes;
}

bool Cartridge::usingPlatformPsramRom() const {
	return _platformPsramRom;
}

std::uint32_t Cartridge::platformPsramRomBytes() const {
	return _platformPsramRomBytes;
}

std::uint8_t const* Cartridge::fixedRomData() const {
	if (_platformFlashRom == nullptr || _platformFlashRomBytes == 0) {
		return nullptr;
	}
	return _platformFlashRom;
}

std::uint8_t const* Cartridge::selectedRomData() const {
	if (_platformFlashRom == nullptr || _selectedRomOffsetBase >= _platformFlashRomBytes) {
		return nullptr;
	}
	return _platformFlashRom + _selectedRomOffsetBase;
}

lexxos::vfs::ReadStatus Cartridge::read8(std::uint16_t address, std::uint8_t& value) {
	value = 0xFF;
	if (!_open) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	if (address < 0x8000U) {
		return readRomOffset(mapRomAddress(address), value);
	}
	return lexxos::vfs::ReadStatus::InvalidPath;
}

std::uint8_t Cartridge::read8Fast(std::uint16_t address) {
	if (!_open || address >= 0x8000U) {
		return 0xFF;
	}
	return readRomOffsetFast(mapRomAddress(address));
}

lexxos::vfs::ReadStatus Cartridge::readExternalRam(std::uint16_t address, std::uint8_t& value) {
	value = 0xFF;
	if (!_open) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	if (address < 0xA000U || address > 0xBFFFU) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}
	if (!_features.externalRam || !_ramEnabled || activeExternalRamSize() == 0) {
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (_mapper == CartridgeMapper::Mbc2) {
		value = static_cast<std::uint8_t>(0xF0U | (_externalRam[mappedExternalRamOffset(address)] & 0x0FU));
		++_externalRamReadCount;
		return lexxos::vfs::ReadStatus::Ok;
	}

	value = _externalRam[mappedExternalRamOffset(address)];
	++_externalRamReadCount;
	return lexxos::vfs::ReadStatus::Ok;
}

std::uint8_t Cartridge::readExternalRamFast(std::uint16_t address) {
	if (!_open || address < 0xA000U || address > 0xBFFFU) {
		return 0xFF;
	}
	if (!_features.externalRam || !_ramEnabled || activeExternalRamSize() == 0) {
		return 0xFF;
	}
	++_externalRamReadCount;
	if (_mapper == CartridgeMapper::Mbc2) {
		return static_cast<std::uint8_t>(0xF0U | (_externalRam[mappedExternalRamOffset(address)] & 0x0FU));
	}
	return _externalRam[mappedExternalRamOffset(address)];
}

void Cartridge::writeExternalRam(std::uint16_t address, std::uint8_t value) {
	if (!_open || address < 0xA000U || address > 0xBFFFU) {
		return;
	}
	if (!_features.externalRam || !_ramEnabled || activeExternalRamSize() == 0) {
		return;
	}

	_externalRam[mappedExternalRamOffset(address)] =
	    _mapper == CartridgeMapper::Mbc2 ? static_cast<std::uint8_t>(value & 0x0FU) : value;
	++_externalRamWriteCount;
}

void Cartridge::write8(std::uint16_t address, std::uint8_t value) {
	if (!_open) {
		return;
	}

	if (_mapper == CartridgeMapper::Mbc2) {
		if (address < 0x4000U) {
			if ((address & 0x0100U) == 0) {
				_ramEnabled = (value & 0x0FU) == 0x0AU;
			} else {
				_mbc2RomBank = static_cast<std::uint8_t>(value & 0x0FU);
				if (_mbc2RomBank == 0) {
					_mbc2RomBank = 1;
				}
				refreshSelectedRomBankCache();
			}
			return;
		}
	}

	if (_mapper == CartridgeMapper::Mbc1) {
		if (address < 0x2000U) {
			_ramEnabled = (value & 0x0FU) == 0x0AU;
			return;
		}
		if (address < 0x4000U) {
			_mbc1RomBankLow5 = static_cast<std::uint8_t>(value & 0x1FU);
			if (_mbc1RomBankLow5 == 0) {
				_mbc1RomBankLow5 = 1;
			}
			refreshSelectedRomBankCache();
			return;
		}
		if (address < 0x6000U) {
			_mbc1BankHigh2 = static_cast<std::uint8_t>(value & 0x03U);
			if (_mbc1Mode != 0) {
				_ramBank = static_cast<std::uint8_t>(_mbc1BankHigh2 % std::max<std::uint32_t>(1, expectedRamBanks()));
			}
			refreshSelectedRomBankCache();
			return;
		}
		if (address < 0x8000U) {
			_mbc1Mode = static_cast<std::uint8_t>(value & 0x01U);
			if (_mbc1Mode == 0) {
				_ramBank = 0;
			}
			return;
		}
	}

	if (_mapper == CartridgeMapper::Mbc5) {
		if (address < 0x2000U) {
			_ramEnabled = (value & 0x0FU) == 0x0AU;
			return;
		}
		if (address < 0x3000U) {
			_mbc5RomBank = static_cast<std::uint16_t>((_mbc5RomBank & 0x100U) | value);
			refreshSelectedRomBankCache();
			return;
		}
		if (address < 0x4000U) {
			_mbc5RomBank = static_cast<std::uint16_t>((_mbc5RomBank & 0x0FFU) | ((value & 0x01U) << 8U));
			refreshSelectedRomBankCache();
			return;
		}
		if (address < 0x6000U) {
			_ramBank = static_cast<std::uint8_t>(value & 0x0FU);
			return;
		}
	}
}

void Cartridge::resetMapperState() {
	_mbc1RomBankLow5 = 1;
	_mbc1BankHigh2 = 0;
	_mbc1Mode = 0;
	_mbc2RomBank = 1;
	_mbc5RomBank = 1;
	_ramBank = 0;
	_ramEnabled = false;

	refreshSelectedRomBankCache();
}

lexxos::vfs::ReadStatus Cartridge::readRomOffset(std::uint32_t offset, std::uint8_t& value) {
	value = 0xFF;
	if (offset >= _fileSize) {
		return lexxos::vfs::ReadStatus::NotFound;
	}

	if (_platformFlashRom != nullptr && offset < _platformFlashRomBytes) {
		value = _platformFlashRom[offset];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (!ensureCacheSlots()) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	auto* const slotsBegin = cacheSlotsBegin();
	auto* const slotsEnd = cacheSlotsEnd();
	if (slotsBegin == nullptr || slotsEnd == nullptr) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}

	auto const cacheBase = offset & ~(static_cast<std::uint32_t>(cacheSize) - 1U);
	auto const index = static_cast<std::size_t>(offset - cacheBase);
	auto const minimumBytes = index + 1U;

	if (_hotCacheSlot != nullptr && _hotCacheSlot->valid && _hotCacheSlot->base == cacheBase && index < _hotCacheSlot->bytes) {
		value = _hotCacheSlot->data[index];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (_previousHotCacheSlot != nullptr && _previousHotCacheSlot->valid && _previousHotCacheSlot->base == cacheBase && index < _previousHotCacheSlot->bytes) {
		auto* const previous = _hotCacheSlot;
		_hotCacheSlot = _previousHotCacheSlot;
		_previousHotCacheSlot = previous;
		_hotCacheSlot->age = ++_cacheAge;
		value = _hotCacheSlot->data[index];
		return lexxos::vfs::ReadStatus::Ok;
	}

	for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
		if (slot->valid && slot->base == cacheBase && index < slot->bytes) {
			_previousHotCacheSlot = _hotCacheSlot;
			_hotCacheSlot = slot;
			slot->age = ++_cacheAge;
			value = slot->data[index];
			return lexxos::vfs::ReadStatus::Ok;
		}
	}

	CacheSlot* target = nullptr;
	for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
		if (!slot->valid) {
			target = slot;
			break;
		}
	}
	if (target == nullptr) {
		for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
			if (slot == _hotCacheSlot || slot == _previousHotCacheSlot) {
				continue;
			}
			if (target == nullptr || slot->age < target->age) {
				target = slot;
			}
		}
	}
	if (target == nullptr) {
		for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
			if (target == nullptr || slot->age < target->age) {
				target = slot;
			}
		}
	}
	if (target == nullptr) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}

	auto const status = fillCacheSlot(*target, cacheBase, minimumBytes);
	if (status != lexxos::vfs::ReadStatus::Ok) {
		return status;
	}

	if (index >= target->bytes) {
		return lexxos::vfs::ReadStatus::NotFound;
	}
	_previousHotCacheSlot = _hotCacheSlot;
	_hotCacheSlot = target;
	value = target->data[index];
	return lexxos::vfs::ReadStatus::Ok;
}

std::uint8_t Cartridge::readRomOffsetFast(std::uint32_t offset) {
	if (offset >= _fileSize) {
		return 0xFF;
	}

	if (_platformFlashRom != nullptr && offset < _platformFlashRomBytes) {
		return _platformFlashRom[offset];
	}

	if (!ensureCacheSlots()) {
		return 0xFF;
	}
	auto* const slotsBegin = cacheSlotsBegin();
	auto* const slotsEnd = cacheSlotsEnd();
	if (slotsBegin == nullptr || slotsEnd == nullptr) {
		return 0xFF;
	}

	auto const cacheBase = offset & ~(static_cast<std::uint32_t>(cacheSize) - 1U);
	auto const index = static_cast<std::size_t>(offset - cacheBase);

	if (_hotCacheSlot != nullptr && _hotCacheSlot->valid && _hotCacheSlot->base == cacheBase && index < _hotCacheSlot->bytes) {
		return _hotCacheSlot->data[index];
	}

	if (_previousHotCacheSlot != nullptr && _previousHotCacheSlot->valid && _previousHotCacheSlot->base == cacheBase && index < _previousHotCacheSlot->bytes) {
		auto* const previous = _hotCacheSlot;
		_hotCacheSlot = _previousHotCacheSlot;
		_previousHotCacheSlot = previous;
		_hotCacheSlot->age = ++_cacheAge;
		return _hotCacheSlot->data[index];
	}

	for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
		if (slot->valid && slot->base == cacheBase && index < slot->bytes) {
			_previousHotCacheSlot = _hotCacheSlot;
			_hotCacheSlot = slot;
			slot->age = ++_cacheAge;
			return slot->data[index];
		}
	}

	std::uint8_t value = 0xFF;
	(void) readRomOffset(offset, value);
	return value;
}

lexxos::vfs::ReadStatus Cartridge::fillCacheSlot(CacheSlot& slot, std::uint32_t cacheBase, std::size_t minimumBytes) {
	lexxos::vfs::ReadStatus lastStatus = lexxos::vfs::ReadStatus::NotReadable;
	auto const fillStartMs = uptimeMs();

#if defined(LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE)
	if (slot.data == nullptr) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	auto* const cacheData = slot.data;
	auto const cacheCapacity = cacheSize;
#else
	auto* const cacheData = slot.data.data();
	auto const cacheCapacity = slot.data.size();
#endif

	auto const availableBytes = _fileSize > cacheBase ? _fileSize - cacheBase : 0U;
	auto const bytesToRead = static_cast<std::size_t>(std::min<std::uint64_t>(cacheCapacity, availableBytes));
	if (bytesToRead < minimumBytes) {
		return lexxos::vfs::ReadStatus::NotFound;
	}

	for (std::uint8_t attempt = 0; attempt < romReadAttempts; ++attempt) {
		std::size_t bytesRead = 0;
		if (_platformPsramRom) {
			auto const& externalMemory = platformServices().externalMemory;
			if (externalMemory.read != nullptr && externalMemory.read(gameBoyRomPsramRegionName, cacheBase, cacheData, bytesToRead)) {
				lastStatus = lexxos::vfs::ReadStatus::Ok;
				bytesRead = bytesToRead;
			} else {
				lastStatus = lexxos::vfs::ReadStatus::NotReadable;
			}
		} else if (!_file.seek(cacheBase)) {
			lastStatus = lexxos::vfs::ReadStatus::NotReadable;
		} else {
			lastStatus = _file.read(cacheData, bytesToRead, bytesRead);
		}
		if (lastStatus == lexxos::vfs::ReadStatus::Ok && bytesRead >= minimumBytes) {
			_cacheReadBytes += static_cast<std::uint64_t>(bytesRead);
			_cacheFillMs += uptimeMs() - fillStartMs;
			slot.base = cacheBase;
			slot.bytes = bytesRead;
			slot.age = ++_cacheAge;
			slot.valid = true;
			++_cacheFillCount;
			return lexxos::vfs::ReadStatus::Ok;
		}
		if (lastStatus == lexxos::vfs::ReadStatus::Ok) {
			lastStatus = lexxos::vfs::ReadStatus::NotFound;
		}

		slot.valid = false;
		++_cacheRetryCount;
		sleepMs(romReadRetryDelayMs);
	}

	++_cacheFailureCount;
	_cacheFillMs += uptimeMs() - fillStartMs;
	return lastStatus;
}

std::uint32_t Cartridge::mapRomAddress(std::uint16_t address) const {
	if (address < 0x4000U) {
		return address;
	}
	return _selectedRomOffsetBase + static_cast<std::uint32_t>(address - 0x4000U);
}

std::uint32_t Cartridge::normalizedSwitchableRomBank(std::uint32_t bank) const {
	auto const banks = expectedRomBanks();
	if (banks == 0) {
		return bank;
	}
	bank %= banks;
	if (bank == 0 && banks > 1) {
		bank = 1;
	}
	return bank;
}

std::size_t Cartridge::activeExternalRamSize() const {
	auto const size = expectedRamSize();
	if (size == 0) {
		return 0;
	}
	return static_cast<std::size_t>(std::min<std::uint32_t>(size, static_cast<std::uint32_t>(externalRamCapacity)));
}

lexxos::platform::PlatformServices const& Cartridge::platformServices() const {
	return _platformServices != nullptr ? *_platformServices : lexxos::platform::services();
}

std::uint32_t Cartridge::uptimeMs() const {
	auto const& clock = platformServices().clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

void Cartridge::sleepMs(std::uint32_t milliseconds) const {
	auto const& clock = platformServices().clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

std::size_t Cartridge::mappedExternalRamOffset(std::uint16_t address) const {
	auto const activeSize = activeExternalRamSize();
	if (activeSize == 0) {
		return 0;
	}

	if (_mapper == CartridgeMapper::Mbc2) {
		return static_cast<std::size_t>(address - 0xA000U) & 0x01FFU;
	}

	auto const bankCount = std::max<std::size_t>(1, activeSize / ramBankSize);
	auto const bank = static_cast<std::size_t>(_ramBank) % bankCount;
	auto offset = bank * ramBankSize + static_cast<std::size_t>(address - 0xA000U);
	if (activeSize < ramBankSize) {
		offset = static_cast<std::size_t>(address - 0xA000U);
	}
	return offset % activeSize;
}

void Cartridge::invalidateCache() {
	auto* const slotsBegin = cacheSlotsBegin();
	auto* const slotsEnd = cacheSlotsEnd();
	if (slotsBegin != nullptr && slotsEnd != nullptr) {
		for (auto* slot = slotsBegin; slot != slotsEnd; ++slot) {
#if defined(LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE)
			auto* const data = slot->data;
			*slot = CacheSlot{};
			slot->data = data;
#else
			*slot = CacheSlot{};
#endif
		}
	}
	_hotCacheSlot = nullptr;
	_previousHotCacheSlot = nullptr;
	_cacheAge = 0;
	_cacheFillCount = 0;
	_cacheRetryCount = 0;
	_cacheFailureCount = 0;
	_cacheReadBytes = 0;
	_cacheFillMs = 0;
}

} // namespace lexxos::gameboy
