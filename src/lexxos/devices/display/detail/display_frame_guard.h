#pragma once

#include <cstdint>

#include <lexxos/devices/display/double_buffered_display_types.h>

namespace lexxos::devices {

class DoubleBufferedDisplay;

namespace detail {

	class DisplayFrameGuard {
	public:
		explicit DisplayFrameGuard(DoubleBufferedDisplay const& display);
		~DisplayFrameGuard();
		DisplayFrameGuard(DisplayFrameGuard const&) = delete;
		DisplayFrameGuard& operator=(DisplayFrameGuard const&) = delete;

	private:
		DisplayFrameSynchronization _sync;
		std::uintptr_t _state{};
	};

} // namespace lexxos::devices::detail

} // namespace lexxos::devices
