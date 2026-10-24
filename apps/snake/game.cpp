#include "game.h"

namespace snake {
namespace {

	bool same(Point a, Point b) {
		return a.x == b.x && a.y == b.y;
	}

	bool opposite(Direction a, Direction b) {
		return (a == Direction::Up && b == Direction::Down)
		    || (a == Direction::Down && b == Direction::Up)
		    || (a == Direction::Left && b == Direction::Right)
		    || (a == Direction::Right && b == Direction::Left);
	}

	Point advance(Point p, Direction d) {
		switch (d) {
			case Direction::Up   : --p.y; break;
			case Direction::Down : ++p.y; break;
			case Direction::Left : --p.x; break;
			case Direction::Right: ++p.x; break;
		}
		return p;
	}

} // namespace

unsigned Game::random() {
	randomState ^= randomState << 13U;
	randomState ^= randomState >> 17U;
	randomState ^= randomState << 5U;
	return randomState;
}

bool Game::occupies(Point p) const {
	for (unsigned i = 0; i < length; ++i) {
		if (same(p, body[i])) {
			return true;
		}
	}
	return false;
}

bool Game::placeFood() {
	unsigned const freeCells = static_cast<unsigned>(maxLength) - length;
	if (freeCells == 0) {
		return false;
	}
	unsigned choice = random() % freeCells;
	for (int y = 1; y < height - 1; ++y) {
		for (int x = 1; x < width - 1; ++x) {
			Point const p{x, y};
			if (occupies(p)) {
				continue;
			}
			if (choice == 0) {
				food = p;
				return true;
			}
			--choice;
		}
	}
	return false;
}

void Game::reset(unsigned seed) {
	randomState = seed != 0 ? seed : 0xA341316CU; // xorshift cannot start at zero.
	length = 3;
	score = 0;
	eaten = 0;
	direction = Direction::Right;
	pending = Direction::Right;
	phase = Phase::Ready;
	turnQueued = false;
	Point const head{width / 2, height / 2};
	for (unsigned i = 0; i < length; ++i) {
		body[i] = {head.x - static_cast<int>(i), head.y};
	}
	(void) placeFood();
}

void Game::start() {
	if (phase == Phase::Ready) {
		phase = Phase::Playing;
	}
}

void Game::turn(Direction next) {
	if (phase == Phase::Won || phase == Phase::Lost || phase == Phase::Paused || turnQueued) {
		return;
	}
	// Compare with the actual direction, not the pending direction. Prevent a
	// pair of rapid key presses from reversing the snake between game ticks.
	if (opposite(next, direction)) {
		return;
	}
	pending = next;
	turnQueued = true;
	start();
}

void Game::togglePause() {
	if (phase == Phase::Playing) {
		phase = Phase::Paused;
	} else if (phase == Phase::Paused) {
		phase = Phase::Playing;
	}
}

unsigned Game::tickIntervalMs() const {
	// Initial 190 ms, 8 ms faster every 4 foods, minimum 86 ms.
	unsigned const reduction = (eaten / 4U) * 8U;
	return reduction >= 104U ? 86U : 190U - reduction;
}

Sound Game::step() {
	if (phase != Phase::Playing) {
		return Sound::None;
	}
	direction = pending;
	turnQueued = false;
	Point const next = advance(body[0], direction);
	if (next.x <= 0 || next.x >= width - 1 || next.y <= 0 || next.y >= height - 1) {
		phase = Phase::Lost;
		return Sound::Crash;
	}

	bool const eating = same(next, food);
	// Without growth the tail moves away *on this tick*; its old cell is free.
	unsigned const occupied = eating ? length : length - 1U;
	for (unsigned i = 0; i < occupied; ++i) {
		if (same(next, body[i])) {
			phase = Phase::Lost;
			return Sound::Crash;
		}
	}

	if (eating && length < static_cast<unsigned>(maxLength)) {
		++length;
	}
	for (unsigned i = length - 1; i > 0; --i) {
		body[i] = body[i - 1];
	}
	body[0] = next;
	if (!eating) {
		return Sound::None;
	}
	++eaten;
	score += 10;
	if (!placeFood()) {
		phase = Phase::Won;
		return Sound::Win;
	}
	return Sound::Eat;
}

} // namespace snake
