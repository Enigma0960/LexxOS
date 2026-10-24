#include "../../apps/snake/game.h"

#include <cassert>

using snake::width;
using snake::height;
using snake::maxLength;
using snake::Direction;
using snake::Phase;
using snake::Sound;
using snake::Point;
using snake::Game;

int main() {
	Game g{};
	g.reset(123U);
	assert(g.phase == Phase::Ready && g.score == 0 && g.length == 3);
	assert(g.body[0].x == width / 2 && g.body[0].y == height / 2);
	assert(g.food.x > 0 && g.food.x < width - 1 && g.food.y > 0 && g.food.y < height - 1);
	assert(!g.occupies(g.food));
	assert(g.tickIntervalMs() == 190);
	assert(g.step() == Sound::None); // No motion on the title screen.

	g.start();
	Point const oldHead = g.body[0];
	assert(g.step() == Sound::None);
	assert(g.body[0].x == oldHead.x + 1 && g.body[0].y == oldHead.y);
	assert(g.length == 3);
	g.togglePause();
	assert(g.phase == Phase::Paused && g.step() == Sound::None);
	g.togglePause();
	assert(g.phase == Phase::Playing);

	g.reset(42);
	g.turn(Direction::Left); // Immediate reversal is refused.
	assert(g.phase == Phase::Ready);
	g.turn(Direction::Up);
	g.turn(Direction::Left); // Only one turn may be queued per frame.
	assert(g.phase == Phase::Playing && g.step() == Sound::None);
	assert(g.body[0].x == width / 2 && g.body[0].y == height / 2 - 1);

	g.reset(7);
	g.food = {g.body[0].x + 1, g.body[0].y};
	g.start();
	assert(g.step() == Sound::Eat);
	assert(g.length == 4 && g.score == 10 && g.eaten == 1);
	assert(!g.occupies(g.food));
	assert(g.tickIntervalMs() == 190);
	g.eaten = 4;
	assert(g.tickIntervalMs() == 182);
	g.eaten = 1000;
	assert(g.tickIntervalMs() == 86);

	g.reset();
	g.phase = Phase::Playing;
	g.body[0] = {1, 1};
	g.direction = g.pending = Direction::Left;
	assert(g.step() == Sound::Crash && g.phase == Phase::Lost);
	assert(g.step() == Sound::None);

	g.reset();
	g.phase = Phase::Playing;
	g.length = 4;
	g.body[0] = {2, 2};
	g.body[1] = {2, 3};
	g.body[2] = {3, 3};
	g.body[3] = {3, 2};
	g.direction = g.pending = Direction::Right;
	g.food = {10, 10};
	assert(g.step() == Sound::None); // Moving into departing tail is legal.
	assert(g.body[0].x == 3 && g.body[0].y == 2);

	g.reset();
	g.phase = Phase::Playing;
	g.length = 4;
	g.body[0] = {2, 2};
	g.body[1] = {2, 3};
	g.body[2] = {3, 3};
	g.body[3] = {3, 2};
	g.direction = g.pending = Direction::Down;
	g.food = {10, 10};
	assert(g.step() == Sound::Crash && g.phase == Phase::Lost);

	// One free cell left. Eating there fills the board and wins.
	g.reset();
	g.phase = Phase::Playing;
	g.length = maxLength - 1;
	g.body[0] = {1, 1};
	g.food = {2, 1};
	unsigned i = 1;
	for (int y = 1; y < height - 1; ++y) {
		for (int x = 1; x < width - 1; ++x) {
			if ((x == 1 && y == 1) || (x == 2 && y == 1)) {
				continue;
			}
			g.body[i++] = {x, y};
		}
	}
	assert(i == g.length);
	g.direction = g.pending = Direction::Right;
	assert(g.step() == Sound::Win);
	assert(g.phase == Phase::Won && g.length == maxLength);
	assert(g.score == 10);
	return 0;
}
