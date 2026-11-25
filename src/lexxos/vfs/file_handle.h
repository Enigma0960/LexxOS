#pragma once

#include <array>

#include <lexxos/vfs/vfs_types.h>

namespace lexxos::vfs {

class FileHandle final {
public:
	FileHandle() = default;
	FileHandle(FileHandle const&) = delete;
	FileHandle& operator=(FileHandle const&) = delete;
	FileHandle(FileHandle&& other) noexcept;
	FileHandle& operator=(FileHandle&& other) noexcept;
	~FileHandle();

	[[nodiscard]] bool isOpen() const;
	[[nodiscard]] bool canRead() const;
	[[nodiscard]] bool canWrite() const;
	[[nodiscard]] std::string_view path() const;
	[[nodiscard]] std::uint64_t position() const;
	[[nodiscard]] std::uint64_t size() const;

	ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead);
	WriteStatus write(std::string_view data);
	bool seek(std::uint64_t position);
	void close();

	// Explicitly finish a writable file and report flush/f_sync/f_close failures.
	// close() and the destructor remain best-effort for existing callers.
	WriteStatus finish();

private:
	friend OpenStatus open(std::string_view path, OpenMode mode, FileHandle& output);
	friend class Vfs;

	enum class Kind : std::uint8_t {
		None,
		DevNull,
		TmpFile,
		TextFile,
		FilesystemRead,
		FilesystemWrite,
	};

	void reset();
	bool setPath(std::string_view path);
	bool setRelativePath(std::string_view path);

	Kind _kind{Kind::None};
	std::array<char, maxPathLength> _path{};
	std::size_t _pathSize{0};
	std::array<char, maxPathLength> _relativePath{};
	std::size_t _relativePathSize{0};
	std::size_t _tmpIndex{0};
	Vfs* _vfs{nullptr};
	ReadOnlyFilesystem* _filesystem{nullptr};
	std::uint64_t _position{0};
	std::uint64_t _size{0};
	bool _readable{false};
	bool _writable{false};
	bool _open{false};
};

} // namespace lexxos::vfs
