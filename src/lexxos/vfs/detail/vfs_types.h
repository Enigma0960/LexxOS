#pragma once

#include <array>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/vfs/vfs_types.h>

namespace lexxos::vfs {

namespace detail {

	inline constexpr std::size_t maxBlockDevices = lexxos::runtime::standardRuntimeProfile.vfs.maxBlockDevices;
	inline constexpr std::size_t maxFilesystemMounts = lexxos::runtime::standardRuntimeProfile.vfs.maxFilesystemMounts;
	inline constexpr std::size_t maxTmpFiles = lexxos::runtime::standardRuntimeProfile.vfs.maxTmpFiles;
	inline constexpr std::size_t maxTmpFileSize = lexxos::runtime::standardRuntimeProfile.vfs.maxTmpFileSizeBytes;
	inline constexpr std::size_t maxFilesystemFactories = lexxos::runtime::standardRuntimeProfile.vfs.maxFilesystemFactories;

	struct FixedString {
		std::array<char, maxPathLength> data{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		bool append(char value) {
			if (size + 1 >= data.size()) {
				return false;
			}

			data[size++] = value;
			data[size] = '\0';
			return true;
		}

		bool append(std::string_view value) {
			for (char const character : value) {
				if (!append(character)) {
					return false;
				}
			}

			return true;
		}

		[[nodiscard]] std::string_view view() const {
			return std::string_view{data.data(), size};
		}
	};

	struct BlockDeviceMount {
		bool used{false};
		std::array<char, maxNameLength> name{};
		std::array<char, maxPathLength> path{};
		lexxos::devices::BlockDevice* device{nullptr};
	};

	struct FilesystemMount {
		bool used{false};
		std::array<char, maxPathLength> source{};
		std::array<char, maxPathLength> path{};
		ReadOnlyFilesystem* filesystem{nullptr};
	};

	struct TmpFile {
		bool used{false};
		std::array<char, maxNameLength> name{};
		std::array<char, maxTmpFileSize> data{};
		std::size_t size{0};
	};

	struct FilesystemFactoryEntry {
		bool used{false};
		std::array<char, maxNameLength> type{};
		FilesystemFactory factory{nullptr};
		void* userData{nullptr};
	};

	struct TextWriterContext {
		TextWriter writer{nullptr};
		void* userData{nullptr};
	};

	struct TextSliceContext {
		char* output{nullptr};
		std::size_t capacity{0};
		std::uint64_t skip{0};
		std::uint64_t position{0};
		std::size_t copied{0};
	};

} // namespace lexxos::vfs::detail

} // namespace lexxos::vfs
