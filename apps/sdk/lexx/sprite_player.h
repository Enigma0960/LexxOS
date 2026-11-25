#pragma once

#include <lexx/animation_timeline.h>
#include <lexx/graphics.h>

namespace lexx::graphics {

// A fixed grid of frames in one LXIM atlas. BlitRegion performs alpha8
// compositing in LexxOS; the guest never holds decoded pixel data.
struct SpriteSheet {
	unsigned handle{0};
	unsigned width{0};
	unsigned height{0};
	unsigned frameWidth{0};
	unsigned frameHeight{0};

	[[nodiscard]] bool valid(AnimationClip const& clip) const {
		return handle != 0U && clip.valid() && frameWidth != 0U && frameHeight != 0U && frameWidth <= width && frameHeight <= height && width <= 65535U && height <= 65535U && clip.firstFrame + clip.frameCount <= (width / frameWidth) * (height / frameHeight);
	}
};

// Generates only the dirty-region erase and the frame blit. The erase color
// is correct for a *uniform* background; textured scenes must redraw their
// own background inside the old rectangle before submitting the sprite.
class SpritePlayer {
public:
	SpritePlayer(SpriteSheet sheet, AnimationClip clip)
	    : _sheet(sheet)
	    , _clip(clip)
	    , _timeline(clip) {}

	[[nodiscard]] bool valid() const { return _sheet.valid(_clip); }

	void start(unsigned now) {
		_timeline.start(now);
		_visible = false;
	}

	void pause(unsigned now) { _timeline.pause(now); }

	void resume(unsigned now) { _timeline.resume(now); }

	void stop() {
		_timeline.stop();
		_visible = false;
	}

	[[nodiscard]] unsigned frame_at(unsigned now) const { return _timeline.frame_at(now); }

	// Returns false on invalid state or inadequate command capacity.
	// On success count is increased by 0, 1 or 2; submit only if it grew.
	// The caller must submit successfully before accepting the new state.
	[[nodiscard]] bool append(Command* commands, unsigned capacity, unsigned& count,
	    int x, int y, unsigned now, unsigned short background) {
		if (!valid() || _pending || commands == nullptr || count > capacity) {
			return false;
		}

		unsigned const frame = _clip.firstFrame + _timeline.frame_at(now);
		if (_visible && _x == x && _y == y && _frame == frame) {
			return true;
		}

		unsigned const needed = _visible ? 2U : 1U;
		if (capacity - count < needed) {
			return false;
		}

		if (_visible) {
			commands[count++] = {static_cast<unsigned>(CommandType::FillRect), _x, _y,
			    static_cast<int>(_sheet.frameWidth), static_cast<int>(_sheet.frameHeight),
			    background};
		}

		unsigned const columns = _sheet.width / _sheet.frameWidth;
		commands[count++] = sprite_region(_sheet.handle, x, y,
		    (frame % columns) * _sheet.frameWidth,
		    (frame / columns) * _sheet.frameHeight,
		    _sheet.frameWidth, _sheet.frameHeight);
		_nextX = x;
		_nextY = y;
		_nextFrame = frame;
		_pending = true;
		return true;
	}

	// Commit the pending position only after graphics::submit() succeeds.
	void committed() {
		if (_pending) {
			_x = _nextX;
			_y = _nextY;
			_frame = _nextFrame;
			_visible = true;
			_pending = false;
		}
	}

	void cancelled() { _pending = false; }

private:
	SpriteSheet _sheet;
	AnimationClip _clip;
	AnimationTimeline _timeline;
	int _x{0}, _y{0}, _nextX{0}, _nextY{0};
	unsigned _frame{0}, _nextFrame{0};
	bool _visible{false}, _pending{false};
};

} // namespace lexx::graphics
