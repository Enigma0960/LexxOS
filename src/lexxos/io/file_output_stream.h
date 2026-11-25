#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/output_stream.h>
#include <lexxos/vfs/file_handle.h>

namespace lexxos::io {

class FileOutputStream final : public OutputStream {
public:
	FileOutputStream() = default;
	FileOutputStream(std::string_view path, lexxos::vfs::WriteMode mode);

	bool open(std::string_view path, lexxos::vfs::WriteMode mode);
	bool write(std::string_view text) override;

	// Вызывается до сообщения об успехе: деструктор не возвращает ошибки I/O.
	bool finish();

	[[nodiscard]] bool ok() const;
	[[nodiscard]] lexxos::vfs::WriteStatus status() const;
	[[nodiscard]] std::string_view path() const;

private:
	lexxos::vfs::FileHandle _file{};
	lexxos::vfs::WriteStatus _status{lexxos::vfs::WriteStatus::InvalidPath};
};

} // namespace lexxos::io
