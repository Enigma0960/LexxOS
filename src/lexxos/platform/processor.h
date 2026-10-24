#pragma once

namespace lexxos::platform {

enum class ProcessorFamily {
	Unknown,
	NativeHost,
	Rp2040,
	Rp2350,
	Esp32,
	Stm32f4,
};

char const* toString(ProcessorFamily family);

} // namespace lexxos::platform
