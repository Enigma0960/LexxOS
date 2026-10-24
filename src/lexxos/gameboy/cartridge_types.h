#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/vfs/vfs.h>

namespace lexxos::gameboy {

enum class CartridgeMapper : std::uint8_t {
	Unsupported,
	RomOnly,
	Mbc1,
	Mbc2,
	Mbc5,
};

struct CartridgeFeatures {
	bool externalRam{false};
	bool battery{false};
	bool timer{false};
	bool rumble{false};
};

struct CartridgeOpenResult {
	bool ok{false};
	lexxos::vfs::OpenStatus openStatus{lexxos::vfs::OpenStatus::Ok};
	lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
	bool tooSmall{false};
	bool invalidHeader{false};
};

struct CartridgeFlashLoadResult {
	bool ok{false};
	bool platformUnavailable{false};
	bool tooLarge{false};
	bool seekFailed{false};
	lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
	bool shortRead{false};
	bool programFailed{false};
	bool noMatchingImage{false};
	std::uint32_t bytesLoaded{0};
	std::uint32_t flashOffset{0};
	std::uint32_t flashCapacity{0};
};

struct CartridgePsramLoadResult {
	bool ok{false};
	bool platformUnavailable{false};
	bool tooLarge{false};
	bool seekFailed{false};
	lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
	bool shortRead{false};
	bool writeFailed{false};
	bool readFailed{false};
	bool noMatchingImage{false};
	bool verifyFailed{false};
	std::uint32_t verifyFailedOffset{0};
	std::uint8_t verifyExpected{0};
	std::uint8_t verifyActual{0};
	std::uint32_t bytesLoaded{0};
	std::uint32_t capacity{0};
	std::uint32_t transferBytes{0};
};

namespace detail {

	template <std::size_t Size>
	struct CartridgeCacheSlot {
		std::array<std::uint8_t, Size> data{};
		std::uint32_t base{0};
		std::size_t bytes{0};
		std::uint32_t age{0};
		bool valid{false};
	};

} // namespace detail

} // namespace lexxos::gameboy
