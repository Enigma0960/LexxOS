#pragma once

#include <array>

#include <lexxos/vfs/vfs_types.h>

namespace lexxos::vfs {

class ReadOnlyFilesystem {
public:
	virtual ~ReadOnlyFilesystem() = default;
	[[nodiscard]] virtual bool isReady() const = 0;
	[[nodiscard]] virtual std::string_view typeName() const = 0;

	[[nodiscard]] virtual bool isReadOnly() const;

	virtual bool stat(std::string_view path, Stat& output) = 0;
	virtual bool list(std::string_view path, DirectoryVisitor visitor, void* userData) = 0;
	virtual ReadStatus read(
	    std::string_view path,
	    std::uint64_t offset,
	    void* buffer,
	    std::size_t capacity,
	    std::size_t& bytesRead) = 0;
	virtual ReadStatus readText(std::string_view path, TextWriter writer, void* userData) = 0;

	virtual WriteStatus writeText(std::string_view path, std::string_view text, WriteMode mode);

	virtual WriteStatus beginWrite(std::string_view path, WriteMode mode);

	virtual WriteStatus write(std::string_view path, std::uint64_t offset, std::string_view data);

	virtual void closeWrite(std::string_view path);

	// Backwards-compatible default for filesystems without a checked close.
	// Writable backends should override to propagate their sync/close status.
	virtual WriteStatus finishWrite(std::string_view path);

	virtual RemoveStatus remove(std::string_view path);

	virtual WriteStatus createDirectory(std::string_view path);

	virtual RemoveStatus removeDirectory(std::string_view path);

	virtual RenameStatus rename(std::string_view source, std::string_view target);

	[[nodiscard]] virtual bool sync();
};

} // namespace lexxos::vfs
