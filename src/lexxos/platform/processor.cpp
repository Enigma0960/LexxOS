#include <lexxos/platform/processor.h>

namespace lexxos::platform {

char const* toString(ProcessorFamily family) {
	switch (family) {
		case ProcessorFamily::NativeHost:
			return "native-host";
		case ProcessorFamily::Rp2040:
			return "rp2040";
		case ProcessorFamily::Rp2350:
			return "rp2350";
		case ProcessorFamily::Esp32:
			return "esp32";
		case ProcessorFamily::Stm32f4:
			return "stm32f4";
		case ProcessorFamily::Unknown:
			return "unknown";
	}

	return "unknown";
}

} // namespace lexxos::platform
