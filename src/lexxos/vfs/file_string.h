#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::vfs {

enum class FileStringTerminator : std::uint8_t {
	None,
	Null,
};

struct FileStringReadOptions {
	std::size_t byteCount{0};
	FileStringTerminator terminator{FileStringTerminator::Null};
};

struct FileStringReadResult {
	ReadStatus status{ReadStatus::Ok};
	std::size_t bytesRead{0};
	std::size_t bytesRequested{0};
	bool bufferTooSmall{false};

	[[nodiscard]] bool ok() const {
		return status == ReadStatus::Ok && !bufferTooSmall;
	}

	[[nodiscard]] bool full() const {
		return ok() && bytesRead == bytesRequested;
	}

	[[nodiscard]] bool shortRead() const {
		return ok() && bytesRead < bytesRequested;
	}

	[[nodiscard]] std::string_view view(char const* buffer) const {
		return buffer != nullptr ? std::string_view{buffer, bytesRead} : std::string_view{};
	}
};

namespace detail {

	inline ReadStatus readStatusFromOpenStatus(OpenStatus status) {
		switch (status) {
			case OpenStatus::Ok:
				return ReadStatus::Ok;
			case OpenStatus::NotFound:
				return ReadStatus::NotFound;
			case OpenStatus::IsDirectory:
				return ReadStatus::IsDirectory;
			case OpenStatus::IsBlockDevice:
				return ReadStatus::IsBlockDevice;
			case OpenStatus::NotReadable:
				return ReadStatus::NotReadable;
			case OpenStatus::InvalidPath:
				return ReadStatus::InvalidPath;
			case OpenStatus::ReadOnlyFilesystem:
			case OpenStatus::NotWritable:
			case OpenStatus::NoSpace:
				return ReadStatus::NotReadable;
		}
		return ReadStatus::NotReadable;
	}

	inline bool needsTerminator(FileStringTerminator terminator) {
		return terminator == FileStringTerminator::Null;
	}

	inline bool hasStringReadCapacity(
	    char const* output,
	    std::size_t outputCapacity,
	    FileStringReadOptions options) {
		auto const terminatorBytes = needsTerminator(options.terminator) ? std::size_t{1} : std::size_t{0};
		if (options.byteCount > static_cast<std::size_t>(-1) - terminatorBytes) {
			return false;
		}

		auto const requiredCapacity = options.byteCount + terminatorBytes;
		return requiredCapacity == 0 || (output != nullptr && outputCapacity >= requiredCapacity);
	}

} // namespace detail

inline FileStringReadResult readString(
    FileHandle& file,
    char* output,
    std::size_t outputCapacity,
    FileStringReadOptions options) {
	FileStringReadResult result{
	    .status = ReadStatus::Ok,
	    .bytesRead = 0,
	    .bytesRequested = options.byteCount,
	    .bufferTooSmall = false,
	};

	if (!detail::hasStringReadCapacity(output, outputCapacity, options)) {
		result.status = ReadStatus::InvalidPath;
		result.bufferTooSmall = true;
		return result;
	}

	if (options.byteCount == 0) {
		if (detail::needsTerminator(options.terminator)) {
			output[0] = '\0';
		}
		return result;
	}

	result.status = file.read(output, options.byteCount, result.bytesRead);
	if (result.status == ReadStatus::Ok && detail::needsTerminator(options.terminator)) {
		output[result.bytesRead] = '\0';
	}
	return result;
}

template <std::size_t N>
inline FileStringReadResult readString(
    FileHandle& file,
    char (&output)[N],
    std::size_t byteCount,
    FileStringTerminator terminator = FileStringTerminator::Null) {
	return readString(file, output, N, FileStringReadOptions{.byteCount = byteCount, .terminator = terminator});
}

inline FileStringReadResult readString(
    Vfs& vfs,
    std::string_view path,
    std::uint64_t offset,
    char* output,
    std::size_t outputCapacity,
    FileStringReadOptions options) {
	FileHandle file;
	auto const openStatus = vfs.open(path, OpenMode::Read, file);
	if (openStatus != OpenStatus::Ok) {
		return FileStringReadResult{
		    .status = detail::readStatusFromOpenStatus(openStatus),
		    .bytesRead = 0,
		    .bytesRequested = options.byteCount,
		    .bufferTooSmall = false,
		};
	}
	if (!file.seek(offset)) {
		return FileStringReadResult{
		    .status = ReadStatus::InvalidPath,
		    .bytesRead = 0,
		    .bytesRequested = options.byteCount,
		    .bufferTooSmall = false,
		};
	}
	return readString(file, output, outputCapacity, options);
}

template <std::size_t N>
inline FileStringReadResult readString(
    Vfs& vfs,
    std::string_view path,
    std::uint64_t offset,
    char (&output)[N],
    std::size_t byteCount,
    FileStringTerminator terminator = FileStringTerminator::Null) {
	return readString(
	    vfs,
	    path,
	    offset,
	    output,
	    N,
	    FileStringReadOptions{.byteCount = byteCount, .terminator = terminator});
}

inline FileStringReadResult readString(
    std::string_view path,
    std::uint64_t offset,
    char* output,
    std::size_t outputCapacity,
    FileStringReadOptions options) {
	return readString(defaultVfs(), path, offset, output, outputCapacity, options);
}

template <std::size_t N>
inline FileStringReadResult readString(
    std::string_view path,
    std::uint64_t offset,
    char (&output)[N],
    std::size_t byteCount,
    FileStringTerminator terminator = FileStringTerminator::Null) {
	return readString(
	    defaultVfs(),
	    path,
	    offset,
	    output,
	    N,
	    FileStringReadOptions{.byteCount = byteCount, .terminator = terminator});
}

} // namespace lexxos::vfs
