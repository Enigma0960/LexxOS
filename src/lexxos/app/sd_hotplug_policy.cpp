#include <lexxos/app/sd_hotplug_policy.h>

namespace lexxos::app {

void SdHotplugPolicy::initialize(bool inserted) {
	_initialized = true;
	_stableInserted = inserted;
	_previousSample = inserted;
	_stableSamples = debounceSamples;
}

bool SdHotplugPolicy::observe(bool sample) {
	if (!_initialized) {
		initialize(sample);
		_stableSamples = 0;
		return false;
	}

	if (sample == _previousSample) {
		if (_stableSamples < debounceSamples) {
			++_stableSamples;
		}
	} else {
		_previousSample = sample;
		_stableSamples = 1;
	}

	if (_stableSamples < debounceSamples || sample == _stableInserted) {
		return false;
	}

	initialize(sample);
	return true;
}

bool SdHotplugPolicy::acceptStable(bool inserted) {
	bool const changed = _initialized && inserted != _stableInserted;
	initialize(inserted);
	return changed;
}

bool SdHotplugPolicy::readStable(SdDetectSource const& source, bool& inserted) {
	if (source.sample == nullptr || source.sleepMs == nullptr || source.stopped == nullptr) {
		return false;
	}

	bool stableSample = source.sample(source.context);
	std::uint8_t stableSamples = 1;
	while (stableSamples < debounceSamples && !source.stopped(source.context)) {
		source.sleepMs(source.context, debounceIntervalMs);
		bool const sample = source.sample(source.context);
		if (sample == stableSample) {
			++stableSamples;
		} else {
			stableSample = sample;
			stableSamples = 1;
		}
	}

	inserted = stableSample;
	return stableSamples >= debounceSamples;
}

} // namespace lexxos::app
