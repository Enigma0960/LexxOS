#include <lexxos/devices/display/ili9488_buffers.h>

namespace lexxos::devices {

bool Ili9488Buffers::valid() const {
	if (!pixels.data() || !repeated.data() || pixels.size() < 3 || repeated.size() < 3 || pixels.size() % 3 || repeated.size() % 3) {
		return false;
	}

	auto first = reinterpret_cast<std::uintptr_t>(pixels.data());
	auto second = reinterpret_cast<std::uintptr_t>(repeated.data());
	// Сравнение разностей не переполняется при проверке пересечения диапазонов.
	return first < second ? second - first >= pixels.size() : first - second >= repeated.size();
}

} // namespace lexxos::devices
