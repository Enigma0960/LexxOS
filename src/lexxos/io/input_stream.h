#pragma once

#include <cstddef>

#include <lexxos/vfs/vfs_types.h>

namespace lexxos::io {

class InputStream {
public:
	virtual ~InputStream() = default;
	virtual lexxos::vfs::ReadStatus read(void* buffer, std::size_t capacity, std::size_t& bytesRead) = 0;
};

InputStream& nullInputStream();

} // namespace lexxos::io
