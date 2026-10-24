#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include <lexxos/vfs/vfs.h>

namespace lexxos::vfs {

enum class FileNumberByteOrder : std::uint8_t {
	Native,
	LittleEndian,
	BigEndian,
};

struct FileNumberReadOptions {
	FileNumberByteOrder byteOrder{FileNumberByteOrder::Native};
};

struct FileNumberReadResult {
	ReadStatus status{ReadStatus::Ok};
	std::size_t bytesRead{0};
	std::size_t bytesRequested{0};

	[[nodiscard]] bool ok() const {
		return status == ReadStatus::Ok;
	}

	[[nodiscard]] bool full() const {
		return ok() && bytesRead == bytesRequested;
	}

	[[nodiscard]] bool shortRead() const {
		return ok() && bytesRead < bytesRequested;
	}

	[[nodiscard]] bool valueReady() const {
		return full();
	}
};

namespace detail {

	inline ReadStatus readNumberStatusFromOpenStatus(OpenStatus status) {
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

	template <typename T>
	struct IsSupportedFileNumber {
		static constexpr bool value =
		    ((std::is_integral_v<T> && !std::is_same_v<T, bool>)
		        || std::is_same_v<T, float>
		        || std::is_same_v<T, double>)
		    && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
	};

	template <std::size_t Size>
	struct UnsignedFileNumberStorage;

	template <>
	struct UnsignedFileNumberStorage<1> {
		using Type = std::uint8_t;
	};

	template <>
	struct UnsignedFileNumberStorage<2> {
		using Type = std::uint16_t;
	};

	template <>
	struct UnsignedFileNumberStorage<4> {
		using Type = std::uint32_t;
	};

	template <>
	struct UnsignedFileNumberStorage<8> {
		using Type = std::uint64_t;
	};

	template <typename T>
	using UnsignedFileNumberStorageT = typename UnsignedFileNumberStorage<sizeof(T)>::Type;

	template <typename Storage, std::size_t Size>
	inline Storage decodeLittleEndian(std::array<std::uint8_t, Size> const& bytes) {
		Storage value = 0;
		for (std::size_t index = 0; index < Size; ++index) {
			value |= static_cast<Storage>(bytes[index]) << (index * 8U);
		}
		return value;
	}

	template <typename Storage, std::size_t Size>
	inline Storage decodeBigEndian(std::array<std::uint8_t, Size> const& bytes) {
		Storage value = 0;
		for (std::size_t index = 0; index < Size; ++index) {
			value = static_cast<Storage>((value << 8U) | bytes[index]);
		}
		return value;
	}

	template <typename T>
	inline void copyRawNumber(UnsignedFileNumberStorageT<T> raw, T& output) {
		if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
			output = static_cast<T>(raw);
		} else {
			std::memcpy(&output, &raw, sizeof(T));
		}
	}

	template <typename T>
	inline void decodeNumber(
	    std::array<std::uint8_t, sizeof(T)> const& bytes,
	    FileNumberByteOrder byteOrder,
	    T& output) {
		static_assert(IsSupportedFileNumber<T>::value, "unsupported file number type");

		if (byteOrder == FileNumberByteOrder::Native) {
			std::memcpy(&output, bytes.data(), sizeof(T));
			return;
		}

		using Storage = UnsignedFileNumberStorageT<T>;
		Storage const raw = byteOrder == FileNumberByteOrder::LittleEndian
		    ? decodeLittleEndian<Storage>(bytes)
		    : decodeBigEndian<Storage>(bytes);
		copyRawNumber(raw, output);
	}

} // namespace detail

template <typename T>
inline FileNumberReadResult readNumber(
    FileHandle& file,
    T& output,
    FileNumberReadOptions options = {}) {
	static_assert(detail::IsSupportedFileNumber<T>::value,
	    "readNumber supports int8/uint8, int16/uint16, int32/uint32, int64/uint64, float and double");

	std::array<std::uint8_t, sizeof(T)> bytes{};
	FileNumberReadResult result{
	    .status = ReadStatus::Ok,
	    .bytesRead = 0,
	    .bytesRequested = sizeof(T),
	};

	result.status = file.read(bytes.data(), bytes.size(), result.bytesRead);
	if (!result.full()) {
		return result;
	}

	detail::decodeNumber(bytes, options.byteOrder, output);
	return result;
}

template <typename T>
inline FileNumberReadResult readNumber(
    FileHandle& file,
    T& output,
    FileNumberByteOrder byteOrder) {
	return readNumber(file, output, FileNumberReadOptions{.byteOrder = byteOrder});
}

template <typename T>
inline FileNumberReadResult readNumber(
    Vfs& vfs,
    std::string_view path,
    std::uint64_t offset,
    T& output,
    FileNumberReadOptions options = {}) {
	FileHandle file;
	auto const openStatus = vfs.open(path, OpenMode::Read, file);
	if (openStatus != OpenStatus::Ok) {
		return FileNumberReadResult{
		    .status = detail::readNumberStatusFromOpenStatus(openStatus),
		    .bytesRead = 0,
		    .bytesRequested = sizeof(T),
		};
	}
	if (!file.seek(offset)) {
		return FileNumberReadResult{
		    .status = ReadStatus::InvalidPath,
		    .bytesRead = 0,
		    .bytesRequested = sizeof(T),
		};
	}
	return readNumber(file, output, options);
}

template <typename T>
inline FileNumberReadResult readNumber(
    Vfs& vfs,
    std::string_view path,
    std::uint64_t offset,
    T& output,
    FileNumberByteOrder byteOrder) {
	return readNumber(vfs, path, offset, output, FileNumberReadOptions{.byteOrder = byteOrder});
}

template <typename T>
inline FileNumberReadResult readNumber(
    std::string_view path,
    std::uint64_t offset,
    T& output,
    FileNumberReadOptions options = {}) {
	return readNumber(defaultVfs(), path, offset, output, options);
}

template <typename T>
inline FileNumberReadResult readNumber(
    std::string_view path,
    std::uint64_t offset,
    T& output,
    FileNumberByteOrder byteOrder) {
	return readNumber(defaultVfs(), path, offset, output, FileNumberReadOptions{.byteOrder = byteOrder});
}

inline FileNumberReadResult readInt8(FileHandle& file, std::int8_t& output) {
	return readNumber(file, output);
}

inline FileNumberReadResult readUInt8(FileHandle& file, std::uint8_t& output) {
	return readNumber(file, output);
}

inline FileNumberReadResult readInt16(
    FileHandle& file,
    std::int16_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readUInt16(
    FileHandle& file,
    std::uint16_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readInt32(
    FileHandle& file,
    std::int32_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readUInt32(
    FileHandle& file,
    std::uint32_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readInt64(
    FileHandle& file,
    std::int64_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readUInt64(
    FileHandle& file,
    std::uint64_t& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readFloat32(
    FileHandle& file,
    float& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

inline FileNumberReadResult readFloat64(
    FileHandle& file,
    double& output,
    FileNumberByteOrder byteOrder = FileNumberByteOrder::Native) {
	return readNumber(file, output, byteOrder);
}

} // namespace lexxos::vfs
