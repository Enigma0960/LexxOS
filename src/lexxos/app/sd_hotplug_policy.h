#pragma once

#include <cstdint>

namespace lexxos::app {

struct SdDetectSource {
	void* context{nullptr};
	bool (*sample)(void*){nullptr};
	void (*sleepMs)(void*, std::uint32_t){nullptr};
	bool (*stopped)(void*){nullptr};
};

// Политика не знает о GPIO, SD backend, EventBus и планировщике.
class SdHotplugPolicy final {
public:
	static constexpr std::uint8_t debounceSamples = 3;
	static constexpr std::uint32_t debounceIntervalMs = 25;

	void initialize(bool inserted);
	// Возвращает true один раз на устойчивую смену состояния.
	[[nodiscard]] bool observe(bool sample);
	[[nodiscard]] bool acceptStable(bool inserted);
	[[nodiscard]] static bool readStable(SdDetectSource const& source, bool& inserted);

private:
	bool _initialized{false};
	bool _stableInserted{false};
	bool _previousSample{false};
	std::uint8_t _stableSamples{0};
};

} // namespace lexxos::app
