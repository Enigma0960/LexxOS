#pragma once

namespace lexxos::devices {
class AudioDevice;
}

namespace lexxos::apps::gameboy_programs::detail {

class GameBoyPcmOutputGuard final {
public:
	explicit GameBoyPcmOutputGuard(lexxos::devices::AudioDevice* audio);

	GameBoyPcmOutputGuard(GameBoyPcmOutputGuard const&) = delete;
	GameBoyPcmOutputGuard& operator=(GameBoyPcmOutputGuard const&) = delete;

	~GameBoyPcmOutputGuard();

private:
	lexxos::devices::AudioDevice* _audio{nullptr};
};

} // namespace lexxos::apps::gameboy_programs::detail
