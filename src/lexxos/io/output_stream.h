#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::io {

class InputStream {
public:
	virtual ~InputStream() = default;

	virtual lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) = 0;
};

class NullInputStream final : public InputStream {
public:
	lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) override;
};

class FixedBufferInputStream final : public InputStream {
public:
	FixedBufferInputStream(std::string_view data);

	lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) override;
	void reset(std::string_view data);

private:
	std::string_view _data{};
	std::size_t _position{0};
};

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

class OutputStream {
public:
	virtual ~OutputStream() = default;

	virtual bool write(std::string_view text) = 0;
	bool writeLine(std::string_view text);
};

class ConsoleOutputStream final : public OutputStream {
public:
	ConsoleOutputStream();
	explicit ConsoleOutputStream(lexxos::platform::PlatformServices const& platform);

	bool write(std::string_view text) override;

private:
	lexxos::platform::PlatformServices const* _platform{nullptr};
	bool _lastWasCarriageReturn{false};
};

class NullOutputStream final : public OutputStream {
public:
	bool write(std::string_view text) override;
};

class FixedBufferOutputStream final : public OutputStream {
public:
	FixedBufferOutputStream(char* buffer, std::size_t capacity);

	bool write(std::string_view text) override;
	void reset();

	[[nodiscard]] std::string_view view() const;
	[[nodiscard]] bool overflowed() const;

private:
	char* _buffer{nullptr};
	std::size_t _capacity{0};
	std::size_t _size{0};
	bool _overflowed{false};
};

class FileOutputStream final : public OutputStream {
public:
	FileOutputStream() = default;
	FileOutputStream(std::string_view path, lexxos::vfs::WriteMode mode);

	bool open(std::string_view path, lexxos::vfs::WriteMode mode);
	bool write(std::string_view text) override;
	[[nodiscard]] bool ok() const;
	[[nodiscard]] lexxos::vfs::WriteStatus status() const;
	[[nodiscard]] std::string_view path() const;

private:
	lexxos::vfs::FileHandle _file{};
	lexxos::vfs::WriteStatus _status{lexxos::vfs::WriteStatus::InvalidPath};
};

InputStream& nullInputStream();
OutputStream& consoleOutputStream();
OutputStream& nullOutputStream();

} // namespace lexxos::io
