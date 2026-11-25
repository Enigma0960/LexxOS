#pragma once

namespace lexxos::devices {

class AudioDevice;

} // namespace lexxos::devices

namespace lexxos::apps::music::midi::detail {

class PcmOutputGuard final {
public:
	explicit PcmOutputGuard(lexxos::devices::AudioDevice& audio);
	PcmOutputGuard(PcmOutputGuard const&) = delete;
	PcmOutputGuard& operator=(PcmOutputGuard const&) = delete;
	~PcmOutputGuard();

private:
	lexxos::devices::AudioDevice* _audio;
};

} // namespace lexxos::apps::music::midi::detail
