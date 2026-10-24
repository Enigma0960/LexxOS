#include "game.h"

namespace pacman {
namespace {

	constexpr char maze[height][width + 1] = {
	    "#########################",
	    "#...........#...........#",
	    "#.###.#####.#.#####.###.#",
	    "#o..#...............#..o#",
	    "###.#.###.#####.###.#.###",
	    "#...#...#...#...#...#...#",
	    "#.#####.###...###.#####.#",
	    "#.......#.......#.......#",
	    "#.#####.###...###.#####.#",
	    "#...#...#...#...#...#...#",
	    "###.#.###.#####.###.#.###",
	    "#o..#...............#..o#",
	    "#.###.#####.#.#####.###.#",
	    "#...........#...........#",
	    "#########################",
	};

	Point adjacent(Point p, Direction direction) {
		switch (direction) {
			case Direction::Up   : --p.y; break;
			case Direction::Down : ++p.y; break;
			case Direction::Left : --p.x; break;
			case Direction::Right: ++p.x; break;
			case Direction::None : break;
		}
		return p;
	}

	bool same(Point left, Point right) {
		return left.x == right.x && left.y == right.y;
	}

	Direction opposite(Direction direction) {
		switch (direction) {
			case Direction::Up   : return Direction::Down;
			case Direction::Down : return Direction::Up;
			case Direction::Left : return Direction::Right;
			case Direction::Right: return Direction::Left;
			default              : return Direction::None;
		}
	}

	int distance(Point left, Point right) {
		int dx = left.x - right.x;
		int dy = left.y - right.y;
		if (dx < 0) {
			dx = -dx;
		}
		if (dy < 0) {
			dy = -dy;
		}
		return dx + dy;
	}

} // namespace

void Game::resetActors() {
	player = {1, 13};
	direction = Direction::None;
	requested = Direction::None;
	ghosts[0] = {
	    {11, 7},
        Direction::Left, {11, 7}
    };
	ghosts[1] = {
	    {13, 7},
        Direction::Right, {13, 7}
    };
	frightenedTicks = 0;
	ticks = 0;
}

void Game::reset() {
	score = 0;
	remaining = 0;
	lives = 3;
	randomState = 0x9E3779B9U;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x <= width; ++x) {
			map[y][x] = maze[y][x];
			if (maze[y][x] == '.' || maze[y][x] == 'o') {
				++remaining;
			}
		}
	}
	resetActors();
	// The starting cell is occupied by the player, not a dot.
	if (map[player.y][player.x] == '.') {
		map[player.y][player.x] = ' ';
		--remaining;
	}
	phase = Phase::Ready;
}

bool Game::walkable(Point p) const {
	return p.x >= 0 && p.x < width && p.y >= 0 && p.y < height && map[p.y][p.x] != '#';
}

void Game::turn(Direction value) {
	if (value == Direction::None) {
		return;
	}
	requested = value;
	if (phase == Phase::Ready) {
		phase = Phase::Playing;
	}
}

void Game::togglePause() {
	if (phase == Phase::Playing) {
		phase = Phase::Paused;
	} else if (phase == Phase::Paused) {
		phase = Phase::Playing;
	}
}

void Game::start() {
	if (phase == Phase::Ready) {
		phase = Phase::Playing;
	}
}

unsigned Game::random() {
	randomState ^= randomState << 13U;
	randomState ^= randomState >> 17U;
	randomState ^= randomState << 5U;
	return randomState;
}

void Game::moveGhost(Ghost& ghost) {
	constexpr Direction choices[] = {Direction::Up, Direction::Left, Direction::Down, Direction::Right};
	Direction best = Direction::None;
	int bestCost = 10000;
	bool const hunt = frightenedTicks == 0;
	for (Direction choice : choices) {
		Point const target = adjacent(ghost.position, choice);
		if (!walkable(target)) {
			continue;
		}
		// Prefer not to reverse unless this is a dead end.
		bool const reverse = choice == opposite(ghost.direction);
		int cost = distance(target, player) * 8 + static_cast<int>(random() & 7U);
		if (!hunt) {
			cost = -cost;
		}
		if (reverse) {
			cost += 100;
		}
		if (cost < bestCost || best == Direction::None) {
			bestCost = cost;
			best = choice;
		}
	}
	if (best != Direction::None) {
		ghost.position = adjacent(ghost.position, best);
		ghost.direction = best;
	}
}

Sound Game::collide() {
	for (Ghost& ghost : ghosts) {
		if (!same(player, ghost.position)) {
			continue;
		}
		if (frightenedTicks != 0) {
			score += 200;
			ghost.position = ghost.home;
			ghost.direction = Direction::None;
			return Sound::Ghost;
		}
		if (lives > 0) {
			--lives;
		}
		resetActors();
		phase = lives == 0 ? Phase::Lost : Phase::Ready;
		return Sound::Hit;
	}
	return Sound::None;
}

Sound Game::step() {
	if (phase != Phase::Playing) {
		return Sound::None;
	}
	++ticks;
	if (requested != Direction::None && walkable(adjacent(player, requested))) {
		direction = requested;
	}
	if (direction != Direction::None && walkable(adjacent(player, direction))) {
		player = adjacent(player, direction);
	}

	Sound sound = Sound::None;
	char& cell = map[player.y][player.x];
	if (cell == '.' || cell == 'o') {
		if (cell == 'o') {
			frightenedTicks = 35; // 35 * 170ms, approximately 6 seconds.
			score += 50;
			sound = Sound::Power;
		} else {
			score += 10;
			sound = Sound::Dot;
		}
		cell = ' ';
		--remaining;
	}

	Sound collision = collide();
	if (collision != Sound::None) {
		return collision;
	}
	if (remaining == 0) {
		phase = Phase::Won;
		return Sound::Win;
	}

	// Ghosts move at half the player's speed. Check after each individual move,
	// so a crossing cannot be hidden by a later move on the same tick.
	if ((ticks & 1U) == 0U) {
		for (Ghost& ghost : ghosts) {
			moveGhost(ghost);
			collision = collide();
			if (collision != Sound::None) {
				return collision;
			}
		}
	}
	if (frightenedTicks > 0) {
		--frightenedTicks;
	}
	return sound;
}

} // namespace pacman
