#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::devices {

class UartBus {
public:
	virtual ~UartBus() = default;

	[[nodiscard]] virtual bool isReady() const = 0;
	virtual bool setBaudRate(std::uint32_t baudRate) = 0;

	// Transfers are synchronous. read() must not wait indefinitely.
	// bytesTransferred is meaningful only on success.
	virtual bool write(std::uint8_t const* data,
	    std::size_t size,
	    std::size_t& bytesTransferred) = 0;

	virtual bool read(std::uint8_t* data,
	    std::size_t capacity,
	    std::size_t& bytesTransferred) = 0;
};

} // namespace lexxos::devices
