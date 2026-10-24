#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/storage/block_device.h>
#include <lexxos/feature.h>
#include <lexxos/vfs/vfs.h>

#if defined(FEATURE_FATFS) && FEATURE_FATFS > 0
extern "C" {
	#include <ff.h>

	#include <diskio.h>
}
#endif

namespace lexxos::fs::fat {

enum class FatMountStatus : std::uint8_t {
	Ok,
	NoDevice,
	DeviceNotReady,
	ReadError,
	UnsupportedSectorSize,
	UnsupportedFatType,
	InvalidBootSector,
	NoSupportedPartition,
	AlreadyMounted,
	NoMountSlot,
};

struct FatVolumeInfo {
	std::string_view source;
	std::string_view target;
	std::string_view type;
	bool ready{false};
	bool readOnly{false};
	std::uint32_t partitionStartBlock{0};
	std::uint32_t partitionBlockCount{0};
	std::uint32_t fatStartBlock{0};
	std::uint32_t fatSizeBlocks{0};
	std::uint32_t rootDirectoryStartBlock{0};
	std::uint32_t rootDirectoryBlocks{0};
	std::uint32_t rootCluster{0};
	std::uint32_t dataStartBlock{0};
	std::uint32_t clusterCount{0};
	std::uint8_t sectorsPerCluster{0};
	std::uint32_t bytesPerCluster{0};
};

using FatVolumeVisitor = bool (*)(FatVolumeInfo const& volume, void* userData);

[[nodiscard]] char const* toString(FatMountStatus status);

class FatVolume final : public lexxos::vfs::ReadOnlyFilesystem {
public:
	static constexpr std::size_t sectorSize = 512;

	FatMountStatus mount(lexxos::devices::BlockDevice& device);
	void reset();

	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] bool isMounted() const;
	[[nodiscard]] bool isMountedAt(std::string_view target) const;
	bool setMountSource(std::string_view source);
	bool setMountTarget(std::string_view target);
	[[nodiscard]] std::string_view mountSource() const;
	[[nodiscard]] std::string_view mountTarget() const;
	[[nodiscard]] std::string_view typeName() const override;
	[[nodiscard]] bool isReadOnly() const override;
	bool stat(std::string_view path, lexxos::vfs::Stat& output) override;
	bool list(std::string_view path, lexxos::vfs::DirectoryVisitor visitor, void* userData) override;
	lexxos::vfs::ReadStatus read(
	    std::string_view path,
	    std::uint64_t offset,
	    void* buffer,
	    std::size_t capacity,
	    std::size_t& bytesRead) override;
	lexxos::vfs::ReadStatus readText(std::string_view path, lexxos::vfs::TextWriter writer, void* userData) override;
	lexxos::vfs::WriteStatus writeText(std::string_view path, std::string_view text, lexxos::vfs::WriteMode mode) override;
	lexxos::vfs::WriteStatus beginWrite(std::string_view path, lexxos::vfs::WriteMode mode) override;
	lexxos::vfs::WriteStatus write(std::string_view path, std::uint64_t offset, std::string_view data) override;
	void closeWrite(std::string_view path) override;
	lexxos::vfs::RemoveStatus remove(std::string_view path) override;
	lexxos::vfs::WriteStatus createDirectory(std::string_view path) override;
	lexxos::vfs::RemoveStatus removeDirectory(std::string_view path) override;
	lexxos::vfs::RenameStatus rename(std::string_view source, std::string_view target) override;
	[[nodiscard]] bool sync() override;

	[[nodiscard]] FatVolumeInfo info() const;

private:
	class OperationGuard {
	public:
		explicit OperationGuard(FatVolume const& volume);
		~OperationGuard();

		[[nodiscard]] bool locked() const;

	private:
		FatVolume const& _volume;
		bool _locked{false};
	};

	[[nodiscard]] bool lockOperation() const;
	void unlockOperation() const;

	[[nodiscard]] bool makeFatPath(std::string_view path, char* output, std::size_t outputCapacity) const;
	[[nodiscard]] bool isCachedReadPath(std::string_view path) const;
	[[nodiscard]] bool isCachedWritePath(std::string_view path) const;
	void closeCachedReadFile();
	[[nodiscard]] lexxos::vfs::WriteStatus flushCachedWriteBuffer();
	[[nodiscard]] lexxos::vfs::WriteStatus closeCachedWriteFile();
	void releaseCachedWriteBuffer();
	[[nodiscard]] lexxos::vfs::ReadStatus ensureCachedReadFile(std::string_view path, char const* fatPath);

	lexxos::devices::BlockDevice* _device{nullptr};
	std::array<char, lexxos::vfs::maxPathLength> _mountSource{};
	std::array<char, lexxos::vfs::maxPathLength> _mountTarget{};
	std::array<char, 8> _drivePath{};
	std::uint8_t _drive{0xFF};
	bool _mounted{false};
	bool _ready{false};

#if defined(LEXXOS_KERNEL_FREERTOS)
	mutable void* _operationLock{nullptr};
	mutable std::atomic<bool> _operationLockCreationGuard{false};
#endif

#if defined(FEATURE_FATFS) && FEATURE_FATFS > 0
	FATFS _filesystem{};
	FIL _cachedReadFile{};
	std::array<char, lexxos::vfs::maxPathLength> _cachedReadPath{};
	std::size_t _cachedReadPathSize{0};
	bool _cachedReadOpen{false};

	FIL _cachedWriteFile{};
	std::array<char, lexxos::vfs::maxPathLength> _cachedWritePath{};
	std::size_t _cachedWritePathSize{0};
	bool _cachedWriteOpen{false};
	bool _cachedWriteDirty{false};
	char* _cachedWriteBuffer{nullptr};
	std::size_t _cachedWriteBufferCapacity{0};
	std::size_t _cachedWriteBufferUsed{0};
#endif
};

[[nodiscard]] FatMountStatus mount(lexxos::vfs::Vfs& vfs, std::string_view source, std::string_view target);
[[nodiscard]] FatMountStatus mount(std::string_view source, std::string_view target);
lexxos::vfs::UnmountStatus unmount(lexxos::vfs::Vfs& vfs, std::string_view target);
lexxos::vfs::UnmountStatus unmount(std::string_view target);
lexxos::vfs::UnmountStatus forceUnmount(lexxos::vfs::Vfs& vfs, std::string_view target);
lexxos::vfs::UnmountStatus forceUnmount(std::string_view target);
[[nodiscard]] bool isMounted(std::string_view target);
bool visitMountedVolumes(FatVolumeVisitor visitor, void* userData);
bool registerVfsFactory(lexxos::vfs::Vfs& vfs);

} // namespace lexxos::fs::fat
