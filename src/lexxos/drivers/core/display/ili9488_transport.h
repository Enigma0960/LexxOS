#pragma once
#include <cstddef>
#include <cstdint>

namespace lexxos::drivers::core {

// Транспорт сохраняет границы command/data; данные действительны только внутри вызова.
struct Ili9488CommandTransport {
	void* context{nullptr};
	bool (*write)(void* context, std::uint8_t command, std::uint8_t const* data, std::size_t size){nullptr};
	void (*sleepMs)(void* context, std::uint32_t milliseconds){nullptr};
};

} // namespace lexxos::drivers::core
