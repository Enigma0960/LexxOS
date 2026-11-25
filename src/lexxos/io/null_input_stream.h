#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/input_stream.h>

namespace lexxos::io {

class NullInputStream final : public InputStream {
public:
	lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) override;
};

} // namespace lexxos::io
