#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/input_stream.h>
#include <lexxos/vfs/file_handle.h>

namespace lexxos::io {

class FileInputStream final : public InputStream {
public:
	FileInputStream() = default;
	FileInputStream(std::string_view path);

	bool open(std::string_view path);
	lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) override;
	[[nodiscard]] bool ok() const;
	[[nodiscard]] lexxos::vfs::OpenStatus status() const;
	[[nodiscard]] std::string_view path() const;

private:
	lexxos::vfs::FileHandle _file{};
	lexxos::vfs::OpenStatus _status{lexxos::vfs::OpenStatus::InvalidPath};
};

} // namespace lexxos::io
