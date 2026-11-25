#pragma once

#include <lexx/lexx.h>

namespace lexx::system {

struct MemoryInfo {
	uint32_t guest_bytes{0};
	uint32_t stack_bytes{0};
};

static_assert(sizeof(MemoryInfo) == 8);

// Reports guest allocation declared in the executable; not physical free RAM.
inline bool memory_info(MemoryInfo& info) {
	return static_cast<int>(detail::ecall1(syscall_memory_info,
	           reinterpret_cast<uint32_t>(&info)))
	    == 0;
}

// UTC seconds. Requires the 'clock' executable capability and a valid OS RTC.
inline bool unix_time(uint32_t& seconds) {
	return static_cast<int>(detail::ecall1(syscall_unix_time,
	           reinterpret_cast<uint32_t>(&seconds)))
	    == 0;
}

} // namespace lexx::system
