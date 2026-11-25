#pragma once

#include <string_view>

namespace lexxos::io {

class OutputStream {
public:
	virtual ~OutputStream() = default;
	virtual bool write(std::string_view text) = 0;
	bool writeLine(std::string_view text);
};

OutputStream& consoleOutputStream();
OutputStream& nullOutputStream();

} // namespace lexxos::io
