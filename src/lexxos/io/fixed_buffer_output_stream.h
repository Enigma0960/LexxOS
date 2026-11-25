#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/io/output_stream.h>

namespace lexxos::io {

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

} // namespace lexxos::io
