#pragma once

// Host-testable timing model. No heap, SDK system calls, or device state.
namespace lexx::graphics {

enum class PlaybackMode : unsigned char {
	Loop,
	PingPong,
	Once
};

struct AnimationClip {
	unsigned firstFrame{0};
	unsigned frameCount{1};
	unsigned frameMs{100};
	PlaybackMode mode{PlaybackMode::Loop};
	// Optional per-frame durations, in milliseconds; must outlive the player.
	unsigned short const* durations{nullptr};

	[[nodiscard]] bool valid() const {
		if (frameCount == 0U || frameCount > 64U || firstFrame > 65535U - frameCount || (durations == nullptr && frameMs == 0U)) {
			return false;
		}

		for (unsigned i = 0; durations != nullptr && i < frameCount; ++i) {
			if (durations[i] == 0U) {
				return false;
			}
		}

		return true;
	}
};

class AnimationTimeline {
public:
	explicit AnimationTimeline(AnimationClip clip)
	    : _clip(clip) {}

	[[nodiscard]] bool valid() const { return _clip.valid(); }

	void start(unsigned now) {
		_started = now;
		_pausedElapsed = 0;
		_running = true;
		_paused = false;
	}

	void pause(unsigned now) {
		if (_running && !_paused) {
			_pausedElapsed = now - _started;
			_paused = true;
		}
	}

	void resume(unsigned now) {
		if (_running && _paused) {
			_started = now - _pausedElapsed;
			_paused = false;
		}
	}

	void stop() {
		_running = false;
		_paused = false;
		_pausedElapsed = 0;
	}

	[[nodiscard]] bool running() const { return _running; }

	[[nodiscard]] bool paused() const { return _paused; }

	[[nodiscard]] unsigned frame_at(unsigned now) const {
		if (!_clip.valid() || !_running || _clip.frameCount == 1U) {
			return 0U;
		}

		unsigned const elapsed = _paused ? _pausedElapsed : now - _started;
		unsigned const count = _clip.frameCount;
		unsigned const steps = _clip.mode == PlaybackMode::PingPong
		    ? 2U * (count - 1U)
		    : count;
		unsigned cycleMs = 0U;
		for (unsigned step = 0; step < steps; ++step) {
			unsigned const index = _clip.mode == PlaybackMode::PingPong && step >= count
			    ? 2U * (count - 1U) - step
			    : step;
			cycleMs += duration(index);
		}

		unsigned remaining = _clip.mode == PlaybackMode::Once && elapsed >= cycleMs
		    ? cycleMs - 1U
		    : elapsed % cycleMs;
		for (unsigned step = 0; step < steps; ++step) {
			unsigned const index = _clip.mode == PlaybackMode::PingPong && step >= count
			    ? 2U * (count - 1U) - step
			    : step;
			unsigned const ms = duration(index);
			if (remaining < ms) {
				return index;
			}

			remaining -= ms;
		}

		return count - 1U;
	}

private:
	[[nodiscard]] unsigned duration(unsigned index) const {
		return _clip.durations != nullptr ? _clip.durations[index] : _clip.frameMs;
	}

	AnimationClip _clip;
	unsigned _started{0};
	unsigned _pausedElapsed{0};
	bool _running{false};
	bool _paused{false};
};

} // namespace lexx::graphics
