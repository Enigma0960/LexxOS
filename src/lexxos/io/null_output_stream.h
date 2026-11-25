#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/output_stream.h>

namespace lexxos::io {

class NullOutputStream final : public OutputStream {
public:
	bool write(std::string_view text) override;
};

} // namespace lexxos::io
