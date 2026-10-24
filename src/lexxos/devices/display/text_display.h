#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::devices {

class TextDisplay {
public:
	virtual ~TextDisplay() = default;

	virtual bool initialize() = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
	virtual void clear() = 0;
	virtual void writeLine(std::uint8_t line, std::string_view text) = 0;
};

} // namespace lexxos::devices
