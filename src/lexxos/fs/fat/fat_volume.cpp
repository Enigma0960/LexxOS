#include <lexxos/fs/fat/fat_volume.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
	#include <task.h>
#endif

#include <lexxos/fs/fat/fatfs/fatfs_diskio.h>

extern "C" {
#include <ff.h>

#include <diskio.h>
}

static_assert(sizeof(TCHAR) == sizeof(char), "LexxOS FatFs backend expects the FatFs narrow/UTF-8 TCHAR API");

namespace {

bool copyText(std::string_view source, char* output, std::size_t outputCapacity) {
	if (outputCapacity == 0 || source.size() + 1 > outputCapacity) {
		return false;
	}
	std::memcpy(output, source.data(), source.size());
	output[source.size()] = '\0';
	return true;
}

bool sameText(std::string_view left, std::string_view right) {
	return left.size() == right.size() && std::memcmp(left.data(), right.data(), left.size()) == 0;
}

std::string_view cStringView(char const* value) {
	return value != nullptr ? std::string_view{value, std::strlen(value)} : std::string_view{};
}

#if defined(LEXXOS_FAT_MOUNT_SLOTS)
static constexpr std::size_t mountedVolumeCount = static_cast<std::size_t>(LEXXOS_FAT_MOUNT_SLOTS);
#else
static constexpr std::size_t mountedVolumeCount = 2;
#endif
static_assert(mountedVolumeCount > 0U);

#if defined(LEXXOS_FATFS_READ_CHUNK_BYTES)
static constexpr std::size_t fatfsReadChunkBytes = static_cast<std::size_t>(LEXXOS_FATFS_READ_CHUNK_BYTES);
#else
static constexpr std::size_t fatfsReadChunkBytes = 4096;
#endif
static_assert(fatfsReadChunkBytes > 0U);

#if defined(LEXXOS_FAT_WRITE_BUFFER_SECTORS)
static constexpr std::size_t fatfsWriteBufferSectors = static_cast<std::size_t>(LEXXOS_FAT_WRITE_BUFFER_SECTORS);
#else
static constexpr std::size_t fatfsWriteBufferSectors = 8;
#endif
static_assert(fatfsWriteBufferSectors <= 32U, "FatFs write buffer is intentionally capped to protect embedded RAM");
static constexpr std::size_t fatfsWriteBufferBytes = fatfsWriteBufferSectors * lexxos::fs::fat::FatVolume::sectorSize;

char* allocateFatWriteBuffer() {
	if constexpr (fatfsWriteBufferBytes == 0U) {
		return nullptr;
	}
#if defined(LEXXOS_KERNEL_FREERTOS)
	return static_cast<char*>(pvPortMalloc(fatfsWriteBufferBytes));
#else
	return static_cast<char*>(std::malloc(fatfsWriteBufferBytes));
#endif
}

void freeFatWriteBuffer(char* buffer) {
	if (buffer == nullptr) {
		return;
	}
#if defined(LEXXOS_KERNEL_FREERTOS)
	vPortFree(buffer);
#else
	std::free(buffer);
#endif
}

std::array<lexxos::fs::fat::FatVolume, mountedVolumeCount>& mountedVolumes() {
	static std::array<lexxos::fs::fat::FatVolume, mountedVolumeCount> volumes{};
	return volumes;
}

lexxos::vfs::ReadStatus mapReadStatus(FRESULT result) {
	switch (result) {
		case FR_OK:
			return lexxos::vfs::ReadStatus::Ok;
		case FR_NO_FILE:
		case FR_NO_PATH:
			return lexxos::vfs::ReadStatus::NotFound;
		case FR_INVALID_NAME:
			return lexxos::vfs::ReadStatus::InvalidPath;
		case FR_DENIED:
			return lexxos::vfs::ReadStatus::IsDirectory;
		default:
			return lexxos::vfs::ReadStatus::NotReadable;
	}
}

lexxos::vfs::WriteStatus mapWriteStatus(FRESULT result) {
	switch (result) {
		case FR_OK:
			return lexxos::vfs::WriteStatus::Ok;
		case FR_NO_FILE:
		case FR_NO_PATH:
			return lexxos::vfs::WriteStatus::NotFound;
		case FR_INVALID_NAME:
			return lexxos::vfs::WriteStatus::InvalidPath;
		case FR_DENIED:
		case FR_WRITE_PROTECTED:
			return lexxos::vfs::WriteStatus::NotWritable;
		case FR_NOT_ENOUGH_CORE:
		case FR_TOO_MANY_OPEN_FILES:
			return lexxos::vfs::WriteStatus::NoSpace;
		default:
			return lexxos::vfs::WriteStatus::NotWritable;
	}
}

lexxos::vfs::RemoveStatus mapRemoveStatus(FRESULT result) {
	switch (result) {
		case FR_OK:
			return lexxos::vfs::RemoveStatus::Ok;
		case FR_NO_FILE:
		case FR_NO_PATH:
			return lexxos::vfs::RemoveStatus::NotFound;
		case FR_INVALID_NAME:
			return lexxos::vfs::RemoveStatus::InvalidPath;
		case FR_DENIED:
			return lexxos::vfs::RemoveStatus::DirectoryNotEmpty;
		case FR_WRITE_PROTECTED:
			return lexxos::vfs::RemoveStatus::NotWritable;
		default:
			return lexxos::vfs::RemoveStatus::NotWritable;
	}
}

lexxos::vfs::RenameStatus mapRenameStatus(FRESULT result) {
	switch (result) {
		case FR_OK:
			return lexxos::vfs::RenameStatus::Ok;
		case FR_NO_FILE:
		case FR_NO_PATH:
			return lexxos::vfs::RenameStatus::SourceNotFound;
		case FR_EXIST:
			return lexxos::vfs::RenameStatus::TargetExists;
		case FR_INVALID_NAME:
			return lexxos::vfs::RenameStatus::InvalidPath;
		case FR_WRITE_PROTECTED:
		case FR_DENIED:
			return lexxos::vfs::RenameStatus::NotWritable;
		default:
			return lexxos::vfs::RenameStatus::NotWritable;
	}
}

lexxos::fs::fat::FatMountStatus mapMountStatus(FRESULT result) {
	switch (result) {
		case FR_OK:
			return lexxos::fs::fat::FatMountStatus::Ok;
		case FR_INVALID_DRIVE:
			return lexxos::fs::fat::FatMountStatus::NoDevice;
		case FR_NOT_READY:
			return lexxos::fs::fat::FatMountStatus::DeviceNotReady;
		case FR_NO_FILESYSTEM:
			return lexxos::fs::fat::FatMountStatus::NoSupportedPartition;
		case FR_DISK_ERR:
		case FR_INT_ERR:
			return lexxos::fs::fat::FatMountStatus::ReadError;
		default:
			return lexxos::fs::fat::FatMountStatus::UnsupportedFatType;
	}
}

char const* fatfsTypeName(FATFS const& filesystem) {
	(void) filesystem;
#if defined(FS_EXFAT)
	if (filesystem.fs_type == FS_EXFAT) {
		return "exfat";
	}
#endif
#if defined(FS_FAT32)
	if (filesystem.fs_type == FS_FAT32) {
		return "fat32";
	}
#endif
#if defined(FS_FAT16)
	if (filesystem.fs_type == FS_FAT16) {
		return "fat16";
	}
#endif
#if defined(FS_FAT12)
	if (filesystem.fs_type == FS_FAT12) {
		return "fat12";
	}
#endif
	return "fatfs";
}

} // namespace

char const* lexxos::fs::fat::toString(FatMountStatus status) {
	switch (status) {
		case FatMountStatus::Ok:
			return "ok";
		case FatMountStatus::NoDevice:
			return "no device";
		case FatMountStatus::DeviceNotReady:
			return "device not ready";
		case FatMountStatus::ReadError:
			return "read error";
		case FatMountStatus::UnsupportedSectorSize:
			return "unsupported sector size";
		case FatMountStatus::UnsupportedFatType:
			return "unsupported FAT type";
		case FatMountStatus::InvalidBootSector:
			return "invalid boot sector";
		case FatMountStatus::NoSupportedPartition:
			return "no supported FAT partition";
		case FatMountStatus::AlreadyMounted:
			return "already mounted";
		case FatMountStatus::NoMountSlot:
			return "no mount slot";
		default:
			return "unknown";
	}
}

lexxos::fs::fat::FatVolume::OperationGuard::OperationGuard(FatVolume const& volume)
    : _volume(volume)
    , _locked(_volume.lockOperation()) {
}

lexxos::fs::fat::FatVolume::OperationGuard::~OperationGuard() {
	if (_locked) {
		_volume.unlockOperation();
	}
}

bool lexxos::fs::fat::FatVolume::OperationGuard::locked() const {
	return _locked;
}

bool lexxos::fs::fat::FatVolume::lockOperation() const {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_operationLock == nullptr) {
		while (_operationLockCreationGuard.exchange(true, std::memory_order_acquire)) {
			if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
				taskYIELD();
			}
		}
		if (_operationLock == nullptr) {
			_operationLock = xSemaphoreCreateRecursiveMutex();
		}
		_operationLockCreationGuard.store(false, std::memory_order_release);
	}
	return _operationLock != nullptr && xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(_operationLock), portMAX_DELAY) == pdTRUE;
#else
	return true;
#endif
}

void lexxos::fs::fat::FatVolume::unlockOperation() const {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_operationLock != nullptr) {
		(void) xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(_operationLock));
	}
#endif
}

lexxos::fs::fat::FatMountStatus lexxos::fs::fat::FatVolume::mount(lexxos::devices::BlockDevice& device) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return FatMountStatus::NoMountSlot;
	}

	reset();
	if (!device.isReady()) {
		return FatMountStatus::DeviceNotReady;
	}
	if (device.blockSizeBytes() != sectorSize) {
		return FatMountStatus::UnsupportedSectorSize;
	}

	BYTE drive = 0xFF;
	if (!fatfs_backend::acquireDrive(device, drive)) {
		return FatMountStatus::NoMountSlot;
	}

	_drive = static_cast<std::uint8_t>(drive);
	_drivePath[0] = static_cast<char>('0' + drive);
	_drivePath[1] = ':';
	_drivePath[2] = '\0';

	FRESULT const result = f_mount(&_filesystem, _drivePath.data(), 1);
	if (result != FR_OK) {
		f_mount(nullptr, _drivePath.data(), 0);
		fatfs_backend::releaseDrive(drive);
		_drive = 0xFF;
		_drivePath = {};
		return mapMountStatus(result);
	}

	_device = &device;
	_mounted = true;
	_ready = true;
	return FatMountStatus::Ok;
}

void lexxos::fs::fat::FatVolume::reset() {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return;
	}

	closeCachedReadFile();
	(void) closeCachedWriteFile();
	if (_drive != 0xFF) {
		f_mount(nullptr, _drivePath.data(), 0);
		fatfs_backend::releaseDrive(static_cast<BYTE>(_drive));
	}
	_device = nullptr;
	_mountSource = {};
	_mountTarget = {};
	_drivePath = {};
	_drive = 0xFF;
	_mounted = false;
	_ready = false;
	_filesystem = {};
}

bool lexxos::fs::fat::FatVolume::isReady() const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	return _ready && _device != nullptr && _device->isReady();
}

bool lexxos::fs::fat::FatVolume::isMounted() const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	return _mounted;
}

bool lexxos::fs::fat::FatVolume::isMountedAt(std::string_view target) const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	return _mounted && sameText(mountTarget(), target);
}

bool lexxos::fs::fat::FatVolume::setMountSource(std::string_view source) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	return copyText(source, _mountSource.data(), _mountSource.size());
}

bool lexxos::fs::fat::FatVolume::setMountTarget(std::string_view target) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	return copyText(target, _mountTarget.data(), _mountTarget.size());
}

std::string_view lexxos::fs::fat::FatVolume::mountSource() const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return {};
	}

	return cStringView(_mountSource.data());
}

std::string_view lexxos::fs::fat::FatVolume::mountTarget() const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return {};
	}

	return cStringView(_mountTarget.data());
}

std::string_view lexxos::fs::fat::FatVolume::typeName() const {
	return "fatfs";
}

bool lexxos::fs::fat::FatVolume::isReadOnly() const {
	return false;
}

bool lexxos::fs::fat::FatVolume::stat(std::string_view path, lexxos::vfs::Stat& output) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		output = {};
		return false;
	}

	output = {};
	if (!isReady()) {
		return false;
	}

	if (path.empty() || path == "/") {
		output.type = lexxos::vfs::NodeType::Directory;
		output.ready = true;
		output.blockSizeBytes = sectorSize;
		return true;
	}

	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return false;
	}

	FILINFO info{};
	FRESULT const result = f_stat(fatPath.data(), &info);
	if (result != FR_OK) {
		return false;
	}

	output.type = (info.fattrib & AM_DIR) != 0 ? lexxos::vfs::NodeType::Directory : lexxos::vfs::NodeType::RegularFile;
	output.sizeBytes = static_cast<std::uint64_t>(info.fsize);
	output.blockSizeBytes = sectorSize;
	output.blockCount = (output.sizeBytes + sectorSize - 1U) / sectorSize;
	output.ready = true;
	output.hasFatAttributes = true;
	output.fatAttributes = info.fattrib;

	// FatFs FILINFO exposes modification date/time only (FAT local time, 2 s precision).
	// A zero/invalid timestamp means unavailable; FAT creation time is not in FILINFO.
	unsigned const month = (info.fdate >> 5U) & 0x0FU;
	unsigned const day = info.fdate & 0x1FU;
	unsigned const hour = (info.ftime >> 11U) & 0x1FU;
	unsigned const minute = (info.ftime >> 5U) & 0x3FU;
	unsigned const second = (info.ftime & 0x1FU) * 2U;

	if (month >= 1U && month <= 12U && day >= 1U && day <= 31U && hour < 24U && minute < 60U && second < 60U) {
		output.modifiedAt.year = static_cast<std::uint16_t>(1980U + ((info.fdate >> 9U) & 0x7FU));
		output.modifiedAt.month = static_cast<std::uint8_t>(month);
		output.modifiedAt.day = static_cast<std::uint8_t>(day);
		output.modifiedAt.hour = static_cast<std::uint8_t>(hour);
		output.modifiedAt.minute = static_cast<std::uint8_t>(minute);
		output.modifiedAt.second = static_cast<std::uint8_t>(second);
		output.modifiedAt.valid = true;
	}

	return true;
}

bool lexxos::fs::fat::FatVolume::list(std::string_view path, lexxos::vfs::DirectoryVisitor visitor, void* userData) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	if (!isReady() || visitor == nullptr) {
		return false;
	}

	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return false;
	}

#if defined(LEXXOS_PLATFORM_ESPIDF)
	FF_DIR directory{};
#else
	DIR directory{};
#endif
	FRESULT result = f_opendir(&directory, fatPath.data());
	if (result != FR_OK) {
		return false;
	}

	bool ok = true;
	while (ok) {
		FILINFO info{};
		result = f_readdir(&directory, &info);
		if (result != FR_OK) {
			ok = false;
			break;
		}
		if (info.fname[0] == '\0') {
			break;
		}
		std::string_view const name{info.fname};
		if (name == "." || name == "..") {
			continue;
		}
		lexxos::vfs::DirectoryEntry const entry{
		    .name = name,
		    .type = (info.fattrib & AM_DIR) != 0 ? lexxos::vfs::NodeType::Directory : lexxos::vfs::NodeType::RegularFile,
		    .sizeBytes = static_cast<std::uint64_t>(info.fsize),
		};
		ok = visitor(entry, userData);
	}
	f_closedir(&directory);
	return ok;
}

lexxos::vfs::ReadStatus lexxos::fs::fat::FatVolume::read(
    std::string_view path,
    std::uint64_t offset,
    void* buffer,
    std::size_t capacity,
    std::size_t& bytesRead) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		bytesRead = 0;
		return lexxos::vfs::ReadStatus::NotReadable;
	}

	bytesRead = 0;
	if (!isReady()) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	if (buffer == nullptr && capacity != 0) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}
	if (capacity == 0) {
		return lexxos::vfs::ReadStatus::Ok;
	}

	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}

	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	auto const openStatus = ensureCachedReadFile(path, fatPath.data());
	if (openStatus != lexxos::vfs::ReadStatus::Ok) {
		return openStatus;
	}

	FIL& file = _cachedReadFile;
	if (f_tell(&file) != static_cast<FSIZE_t>(offset)) {
		FRESULT const seekResult = f_lseek(&file, static_cast<FSIZE_t>(offset));
		if (seekResult != FR_OK) {
			closeCachedReadFile();
			return mapReadStatus(seekResult);
		}
	}

	auto* output = static_cast<std::uint8_t*>(buffer);
	std::size_t totalRead = 0;
	while (totalRead < capacity) {
		std::size_t const remaining = capacity - totalRead;
		UINT const chunk = static_cast<UINT>(std::min<std::size_t>(
		    remaining,
		    std::min<std::size_t>(fatfsReadChunkBytes, static_cast<std::size_t>(UINT_MAX))));
		UINT actual = 0;
		FRESULT const readResult = f_read(&file, output + totalRead, chunk, &actual);
		if (readResult != FR_OK) {
			closeCachedReadFile();
			return mapReadStatus(readResult);
		}
		totalRead += static_cast<std::size_t>(actual);
		if (actual == 0 || actual < chunk) {
			break;
		}
	}

	bytesRead = totalRead;
	return lexxos::vfs::ReadStatus::Ok;
}

lexxos::vfs::ReadStatus lexxos::fs::fat::FatVolume::readText(std::string_view path, lexxos::vfs::TextWriter writer, void* userData) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}

	if (!isReady()) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}
	if (writer == nullptr) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}
	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::ReadStatus::NotReadable;
	}

	FIL file{};
	FRESULT result = f_open(&file, fatPath.data(), FA_READ);
	if (result != FR_OK) {
		return mapReadStatus(result);
	}

	std::array<char, 128> buffer{};
	while (true) {
		UINT actual = 0;
		result = f_read(&file, buffer.data(), static_cast<UINT>(buffer.size()), &actual);
		if (result != FR_OK) {
			f_close(&file);
			return mapReadStatus(result);
		}
		if (actual == 0) {
			break;
		}
		writer(std::string_view{buffer.data(), static_cast<std::size_t>(actual)}, userData);
	}

	f_close(&file);
	return lexxos::vfs::ReadStatus::Ok;
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::writeText(std::string_view path, std::string_view text, lexxos::vfs::WriteMode mode) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}

	auto const beginStatus = beginWrite(path, mode);
	if (beginStatus != lexxos::vfs::WriteStatus::Ok) {
		return beginStatus;
	}
	auto const writeStatus = write(path, mode == lexxos::vfs::WriteMode::Append ? f_size(&_cachedWriteFile) : 0, text);
	if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
		(void) closeCachedWriteFile();
		return writeStatus;
	}
	return closeCachedWriteFile();
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::beginWrite(std::string_view path, lexxos::vfs::WriteMode mode) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::WriteStatus::InvalidPath;
	}

	closeCachedReadFile();
	if (_cachedWriteOpen) {
		auto const closeStatus = closeCachedWriteFile();
		if (closeStatus != lexxos::vfs::WriteStatus::Ok) {
			return closeStatus;
		}
	}
	if (path.size() + 1 > _cachedWritePath.size()) {
		return lexxos::vfs::WriteStatus::InvalidPath;
	}

	fatfs_backend::invalidateDiskCache();

	BYTE flags = FA_WRITE;
	if (mode == lexxos::vfs::WriteMode::Append) {
		flags |= FA_OPEN_APPEND;
	} else {
		flags |= FA_CREATE_ALWAYS;
	}

	FRESULT const result = f_open(&_cachedWriteFile, fatPath.data(), flags);
	if (result != FR_OK) {
		_cachedWriteFile = {};
		return mapWriteStatus(result);
	}

	std::memcpy(_cachedWritePath.data(), path.data(), path.size());
	_cachedWritePath[path.size()] = '\0';
	_cachedWritePathSize = path.size();
	_cachedWriteOpen = true;
	_cachedWriteDirty = false;
	_cachedWriteBufferUsed = 0;
	if constexpr (fatfsWriteBufferBytes != 0U) {
		_cachedWriteBuffer = allocateFatWriteBuffer();
		_cachedWriteBufferCapacity = _cachedWriteBuffer != nullptr ? fatfsWriteBufferBytes : 0U;
	}
	return lexxos::vfs::WriteStatus::Ok;
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::write(std::string_view path, std::uint64_t offset, std::string_view data) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}
	if (data.empty()) {
		return lexxos::vfs::WriteStatus::Ok;
	}
	if (!isCachedWritePath(path)) {
		auto const beginStatus = beginWrite(path, lexxos::vfs::WriteMode::Append);
		if (beginStatus != lexxos::vfs::WriteStatus::Ok) {
			return beginStatus;
		}
	}

	FIL& file = _cachedWriteFile;
	auto const bufferedPosition = static_cast<FSIZE_t>(f_tell(&file) + _cachedWriteBufferUsed);
	if (bufferedPosition != static_cast<FSIZE_t>(offset)) {
		auto const flushStatus = flushCachedWriteBuffer();
		if (flushStatus != lexxos::vfs::WriteStatus::Ok) {
			(void) closeCachedWriteFile();
			return flushStatus;
		}
		FRESULT const seekResult = f_lseek(&file, static_cast<FSIZE_t>(offset));
		if (seekResult != FR_OK) {
			(void) closeCachedWriteFile();
			return mapWriteStatus(seekResult);
		}
	}

	std::size_t consumed = 0;
	while (consumed < data.size()) {
		std::size_t const remaining = data.size() - consumed;
		if (_cachedWriteBuffer == nullptr || _cachedWriteBufferCapacity == 0U) {
			UINT const chunk = static_cast<UINT>(std::min<std::size_t>(remaining, static_cast<std::size_t>(UINT_MAX)));
			UINT actual = 0;
			FRESULT const result = f_write(&file, data.data() + consumed, chunk, &actual);
			if (result != FR_OK) {
				(void) closeCachedWriteFile();
				return mapWriteStatus(result);
			}
			consumed += static_cast<std::size_t>(actual);
			_cachedWriteDirty = true;
			if (actual != chunk) {
				(void) closeCachedWriteFile();
				return lexxos::vfs::WriteStatus::NoSpace;
			}
			continue;
		}

		if (_cachedWriteBufferUsed == 0U && remaining >= _cachedWriteBufferCapacity) {
			std::size_t const directBytes = remaining - (remaining % _cachedWriteBufferCapacity);
			std::size_t directWritten = 0;
			while (directWritten < directBytes) {
				std::size_t const directRemaining = directBytes - directWritten;
				UINT const chunk = static_cast<UINT>(std::min<std::size_t>(directRemaining, static_cast<std::size_t>(UINT_MAX)));
				UINT actual = 0;
				FRESULT const result = f_write(&file, data.data() + consumed + directWritten, chunk, &actual);
				if (result != FR_OK) {
					(void) closeCachedWriteFile();
					return mapWriteStatus(result);
				}
				directWritten += static_cast<std::size_t>(actual);
				_cachedWriteDirty = true;
				if (actual != chunk) {
					(void) closeCachedWriteFile();
					return lexxos::vfs::WriteStatus::NoSpace;
				}
			}
			consumed += directBytes;
			continue;
		}

		std::size_t const freeSpace = _cachedWriteBufferCapacity - _cachedWriteBufferUsed;
		std::size_t const copyBytes = std::min<std::size_t>(remaining, freeSpace);
		std::memcpy(_cachedWriteBuffer + _cachedWriteBufferUsed, data.data() + consumed, copyBytes);
		_cachedWriteBufferUsed += copyBytes;
		consumed += copyBytes;
		if (_cachedWriteBufferUsed == _cachedWriteBufferCapacity) {
			auto const flushStatus = flushCachedWriteBuffer();
			if (flushStatus != lexxos::vfs::WriteStatus::Ok) {
				(void) closeCachedWriteFile();
				return flushStatus;
			}
		}
	}

	return lexxos::vfs::WriteStatus::Ok;
}

void lexxos::fs::fat::FatVolume::closeWrite(std::string_view path) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return;
	}

	if (path.empty() || isCachedWritePath(path)) {
		(void) closeCachedWriteFile();
	}
}

lexxos::vfs::RemoveStatus lexxos::fs::fat::FatVolume::remove(std::string_view path) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::RemoveStatus::InvalidPath;
	}

	closeCachedReadFile();
	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}

	FILINFO info{};
	FRESULT result = f_stat(fatPath.data(), &info);
	if (result != FR_OK) {
		return mapRemoveStatus(result);
	}
	if ((info.fattrib & AM_DIR) != 0) {
		return lexxos::vfs::RemoveStatus::IsDirectory;
	}
	return mapRemoveStatus(f_unlink(fatPath.data()));
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::createDirectory(std::string_view path) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::WriteStatus::InvalidPath;
	}
	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::WriteStatus::NotWritable;
	}
	return mapWriteStatus(f_mkdir(fatPath.data()));
}

lexxos::vfs::RemoveStatus lexxos::fs::fat::FatVolume::removeDirectory(std::string_view path) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> fatPath{};
	if (!makeFatPath(path, fatPath.data(), fatPath.size())) {
		return lexxos::vfs::RemoveStatus::InvalidPath;
	}
	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::RemoveStatus::NotWritable;
	}

	FILINFO info{};
	FRESULT result = f_stat(fatPath.data(), &info);
	if (result != FR_OK) {
		return mapRemoveStatus(result);
	}
	if ((info.fattrib & AM_DIR) == 0) {
		return lexxos::vfs::RemoveStatus::NotFound;
	}
	return mapRemoveStatus(f_unlink(fatPath.data()));
}

lexxos::vfs::RenameStatus lexxos::fs::fat::FatVolume::rename(std::string_view source, std::string_view target) {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return lexxos::vfs::RenameStatus::NotWritable;
	}

	if (!isReady()) {
		return lexxos::vfs::RenameStatus::NotWritable;
	}
	std::array<char, lexxos::vfs::maxPathLength + 8> sourcePath{};
	std::array<char, lexxos::vfs::maxPathLength + 8> targetPath{};
	if (!makeFatPath(source, sourcePath.data(), sourcePath.size()) || !makeFatPath(target, targetPath.data(), targetPath.size())) {
		return lexxos::vfs::RenameStatus::InvalidPath;
	}

	closeCachedReadFile();
	if (closeCachedWriteFile() != lexxos::vfs::WriteStatus::Ok) {
		return lexxos::vfs::RenameStatus::NotWritable;
	}

	FILINFO existing{};
	if (f_stat(targetPath.data(), &existing) == FR_OK) {
		return lexxos::vfs::RenameStatus::TargetExists;
	}
	return mapRenameStatus(f_rename(sourcePath.data(), targetPath.data()));
}

bool lexxos::fs::fat::FatVolume::sync() {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return false;
	}

	if (!isReady()) {
		return false;
	}
	closeCachedReadFile();
	return closeCachedWriteFile() == lexxos::vfs::WriteStatus::Ok;
}

lexxos::fs::fat::FatVolumeInfo lexxos::fs::fat::FatVolume::info() const {
	OperationGuard guard{*this};
	if (!guard.locked()) {
		return {};
	}

	FatVolumeInfo result{};
	result.source = mountSource();
	result.target = mountTarget();
	result.type = typeName();
	result.ready = isReady();
	result.readOnly = isReadOnly();
	if (_device != nullptr) {
		std::uint64_t const blocks = _device->blockCount();
		result.partitionBlockCount = blocks > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(blocks);
	}
	if (_ready) {
		result.type = fatfsTypeName(_filesystem);
		result.fatStartBlock = static_cast<std::uint32_t>(_filesystem.fatbase);
		result.fatSizeBlocks = static_cast<std::uint32_t>(_filesystem.fsize);
		result.rootDirectoryStartBlock = static_cast<std::uint32_t>(_filesystem.dirbase);
		result.dataStartBlock = static_cast<std::uint32_t>(_filesystem.database);
		result.clusterCount = _filesystem.n_fatent > 2 ? static_cast<std::uint32_t>(_filesystem.n_fatent - 2) : 0;
		result.sectorsPerCluster = static_cast<std::uint8_t>(_filesystem.csize);
		result.bytesPerCluster = static_cast<std::uint32_t>(_filesystem.csize) * sectorSize;
	}
	return result;
}

bool lexxos::fs::fat::FatVolume::makeFatPath(std::string_view path, char* output, std::size_t outputCapacity) const {
	if (!_mounted || output == nullptr || outputCapacity == 0 || _drivePath[0] == '\0') {
		return false;
	}

	std::size_t size = 0;
	auto append = [&](char value) -> bool {
		if (size + 1 >= outputCapacity) {
			return false;
		}
		output[size++] = value;
		output[size] = '\0';
		return true;
	};
	auto appendText = [&](std::string_view value) -> bool {
		for (char const character : value) {
			if (!append(character)) {
				return false;
			}
		}
		return true;
	};

	if (!appendText(_drivePath.data())) {
		return false;
	}
	if (path.empty() || path == "/") {
		return append('/');
	}
	if (path.front() != '/') {
		if (!append('/')) {
			return false;
		}
	}
	return appendText(path);
}

bool lexxos::fs::fat::FatVolume::isCachedReadPath(std::string_view path) const {
	return _cachedReadOpen && path.size() == _cachedReadPathSize && std::memcmp(_cachedReadPath.data(), path.data(), path.size()) == 0;
}

bool lexxos::fs::fat::FatVolume::isCachedWritePath(std::string_view path) const {
	return _cachedWriteOpen && path.size() == _cachedWritePathSize && std::memcmp(_cachedWritePath.data(), path.data(), path.size()) == 0;
}

void lexxos::fs::fat::FatVolume::closeCachedReadFile() {
	if (!_cachedReadOpen) {
		return;
	}
	f_close(&_cachedReadFile);
	_cachedReadFile = {};
	_cachedReadPath = {};
	_cachedReadPathSize = 0;
	_cachedReadOpen = false;
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::flushCachedWriteBuffer() {
	if (!_cachedWriteOpen || _cachedWriteBuffer == nullptr || _cachedWriteBufferUsed == 0U) {
		return lexxos::vfs::WriteStatus::Ok;
	}

	std::size_t flushed = 0;
	while (flushed < _cachedWriteBufferUsed) {
		std::size_t const remaining = _cachedWriteBufferUsed - flushed;
		UINT const chunk = static_cast<UINT>(std::min<std::size_t>(remaining, static_cast<std::size_t>(UINT_MAX)));
		UINT actual = 0;
		FRESULT const result = f_write(&_cachedWriteFile, _cachedWriteBuffer + flushed, chunk, &actual);
		if (result != FR_OK) {
			_cachedWriteBufferUsed = 0;
			return mapWriteStatus(result);
		}
		flushed += static_cast<std::size_t>(actual);
		_cachedWriteDirty = true;
		if (actual != chunk) {
			_cachedWriteBufferUsed = 0;
			return lexxos::vfs::WriteStatus::NoSpace;
		}
	}
	_cachedWriteBufferUsed = 0;
	return lexxos::vfs::WriteStatus::Ok;
}

void lexxos::fs::fat::FatVolume::releaseCachedWriteBuffer() {
	freeFatWriteBuffer(_cachedWriteBuffer);
	_cachedWriteBuffer = nullptr;
	_cachedWriteBufferCapacity = 0;
	_cachedWriteBufferUsed = 0;
}

lexxos::vfs::WriteStatus lexxos::fs::fat::FatVolume::closeCachedWriteFile() {
	if (!_cachedWriteOpen) {
		releaseCachedWriteBuffer();
		return lexxos::vfs::WriteStatus::Ok;
	}

	auto const flushStatus = flushCachedWriteBuffer();
	FRESULT syncResult = FR_OK;
	if (flushStatus == lexxos::vfs::WriteStatus::Ok && _cachedWriteDirty) {
		syncResult = f_sync(&_cachedWriteFile);
	}
	FRESULT const closeResult = f_close(&_cachedWriteFile);
	fatfs_backend::invalidateDiskCache();
	_cachedWriteFile = {};
	_cachedWritePath = {};
	_cachedWritePathSize = 0;
	_cachedWriteOpen = false;
	_cachedWriteDirty = false;
	releaseCachedWriteBuffer();
	if (flushStatus != lexxos::vfs::WriteStatus::Ok) {
		return flushStatus;
	}
	if (syncResult != FR_OK) {
		return mapWriteStatus(syncResult);
	}
	return closeResult == FR_OK ? lexxos::vfs::WriteStatus::Ok : mapWriteStatus(closeResult);
}

lexxos::vfs::ReadStatus lexxos::fs::fat::FatVolume::ensureCachedReadFile(std::string_view path, char const* fatPath) {
	if (isCachedReadPath(path)) {
		return lexxos::vfs::ReadStatus::Ok;
	}

	closeCachedReadFile();
	if (path.size() + 1 > _cachedReadPath.size()) {
		return lexxos::vfs::ReadStatus::InvalidPath;
	}
	FRESULT const result = f_open(&_cachedReadFile, fatPath, FA_READ);
	if (result != FR_OK) {
		_cachedReadFile = {};
		return mapReadStatus(result);
	}
	std::memcpy(_cachedReadPath.data(), path.data(), path.size());
	_cachedReadPath[path.size()] = '\0';
	_cachedReadPathSize = path.size();
	_cachedReadOpen = true;
	return lexxos::vfs::ReadStatus::Ok;
}

lexxos::fs::fat::FatMountStatus lexxos::fs::fat::mount(
    lexxos::vfs::Vfs& vfs,
    std::string_view source,
    std::string_view target) {
	auto& volumes = mountedVolumes();

	if (isMounted(target)) {
		return FatMountStatus::AlreadyMounted;
	}

	auto* device = vfs.blockDevice(source);
	if (device == nullptr) {
		return FatMountStatus::NoDevice;
	}
	if (!device->isReady()) {
		return FatMountStatus::DeviceNotReady;
	}

	for (auto& volume : volumes) {
		if (volume.isMounted()) {
			continue;
		}

		FatMountStatus const status = volume.mount(*device);
		if (status != FatMountStatus::Ok) {
			return status;
		}

		auto const mountStatus = vfs.mountFilesystem(source, target, &volume);
		if (mountStatus == lexxos::vfs::MountStatus::Ok) {
			if (!volume.setMountSource(source) || !volume.setMountTarget(target)) {
				vfs.unmountFilesystem(target);
				volume.reset();
				return FatMountStatus::NoMountSlot;
			}
			return FatMountStatus::Ok;
		}

		volume.reset();
		if (mountStatus == lexxos::vfs::MountStatus::TargetBusy) {
			return FatMountStatus::AlreadyMounted;
		}
		return FatMountStatus::NoMountSlot;
	}

	return FatMountStatus::NoMountSlot;
}

lexxos::fs::fat::FatMountStatus lexxos::fs::fat::mount(std::string_view source, std::string_view target) {
	return mount(lexxos::vfs::defaultVfs(), source, target);
}

lexxos::vfs::UnmountStatus lexxos::fs::fat::unmount(lexxos::vfs::Vfs& vfs, std::string_view target) {
	auto const status = vfs.unmountFilesystem(target);
	if (status != lexxos::vfs::UnmountStatus::Ok) {
		return status;
	}

	auto& volumes = mountedVolumes();
	for (auto& volume : volumes) {
		if (volume.isMountedAt(target) || volume.mountTarget() == target) {
			volume.reset();
		}
	}

	return status;
}

lexxos::vfs::UnmountStatus lexxos::fs::fat::unmount(std::string_view target) {
	return unmount(lexxos::vfs::defaultVfs(), target);
}

lexxos::vfs::UnmountStatus lexxos::fs::fat::forceUnmount(lexxos::vfs::Vfs& vfs, std::string_view target) {
	auto& volumes = mountedVolumes();
	auto const status = vfs.forceUnmountFilesystem(target);
	for (auto& volume : volumes) {
		if (volume.isMountedAt(target) || volume.mountTarget() == target) {
			volume.reset();
		}
	}
	return status;
}

lexxos::vfs::UnmountStatus lexxos::fs::fat::forceUnmount(std::string_view target) {
	return forceUnmount(lexxos::vfs::defaultVfs(), target);
}

bool lexxos::fs::fat::isMounted(std::string_view target) {
	for (auto const& volume : mountedVolumes()) {
		if (volume.isMountedAt(target)) {
			return true;
		}
	}

	return false;
}

bool lexxos::fs::fat::visitMountedVolumes(FatVolumeVisitor visitor, void* userData) {
	if (visitor == nullptr) {
		return false;
	}
	for (auto const& volume : mountedVolumes()) {
		if (!volume.isMounted()) {
			continue;
		}
		if (!visitor(volume.info(), userData)) {
			return false;
		}
	}
	return true;
}

bool lexxos::fs::fat::registerVfsFactory(lexxos::vfs::Vfs& vfs) {
	auto const factory = [](lexxos::vfs::Vfs& targetVfs,
	                         std::string_view source,
	                         std::string_view target,
	                         void*) -> lexxos::vfs::MountStatus {
		switch (mount(targetVfs, source, target)) {
			case FatMountStatus::Ok:
				return lexxos::vfs::MountStatus::Ok;
			case FatMountStatus::NoDevice:
				return lexxos::vfs::MountStatus::SourceNotFound;
			case FatMountStatus::AlreadyMounted:
				return lexxos::vfs::MountStatus::TargetBusy;
			case FatMountStatus::NoMountSlot:
				return lexxos::vfs::MountStatus::NoMountSlot;
			case FatMountStatus::DeviceNotReady:
			case FatMountStatus::ReadError:
			case FatMountStatus::UnsupportedSectorSize:
			case FatMountStatus::UnsupportedFatType:
			case FatMountStatus::InvalidBootSector:
			case FatMountStatus::NoSupportedPartition:
				return lexxos::vfs::MountStatus::InvalidSource;
		}
		return lexxos::vfs::MountStatus::InvalidSource;
	};

	return vfs.registerFilesystemFactory("fat", factory)
	    && vfs.registerFilesystemFactory("fat16", factory)
	    && vfs.registerFilesystemFactory("fat32", factory);
}
