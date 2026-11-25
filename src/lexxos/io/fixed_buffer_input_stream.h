#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/input_stream.h>

namespace lexxos::io {

class FixedBufferInputStream final : public InputStream {
public:
	FixedBufferInputStream(std::string_view data);

	lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) override;
	void reset(std::string_view data);

private:
	std::string_view _data{};
	std::size_t _position{0};
};

} // namespace lexxos::io
