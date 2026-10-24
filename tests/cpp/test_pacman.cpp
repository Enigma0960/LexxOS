#include "../../apps/pacman/game.h"

#include <cassert>
#include <queue>

using pacman::width;
using pacman::height;
using pacman::Direction;
using pacman::Phase;
using pacman::Sound;
using pacman::Point;
using pacman::Ghost;
using pacman::Game;

int main() {
	Game game{};
	game.reset();
	assert(game.phase == Phase::Ready && game.lives == 3 && game.score == 0);
	assert(!game.walkable({-1, 1}) && !game.walkable({25, 1}));
	assert(!game.walkable({0, 0}) && game.walkable({1, 13}));

	// All collectibles must be reachable from the start.
	bool visited[height][width]{};
	std::queue<Point> q;
	q.push(game.player);
	visited[game.player.y][game.player.x] = true;
	unsigned reachable = 0;
	unsigned collectibles = 0;
	while (!q.empty()) {
		Point p = q.front();
		q.pop();
		++reachable;
		if (game.map[p.y][p.x] == '.' || game.map[p.y][p.x] == 'o') {
			++collectibles;
		}
		Point candidates[] = {
		    {p.x - 1,     p.y},
            {p.x + 1,     p.y},
            {    p.x, p.y - 1},
            {    p.x, p.y + 1}
        };
		for (Point next : candidates) {
			if (game.walkable(next) && !visited[next.y][next.x]) {
				visited[next.y][next.x] = true;
				q.push(next);
			}
		}
	}
	assert(reachable > 100 && collectibles == game.remaining);

	game.turn(Direction::Right);
	assert(game.phase == Phase::Playing);
	assert(game.step() == Sound::Dot);
	assert(game.score == 10 && game.player.x == 2);
	game.togglePause();
	assert(game.phase == Phase::Paused && game.step() == Sound::None);
	assert(game.player.x == 2);
	game.togglePause();
	assert(game.phase == Phase::Playing);

	game.reset();
	game.player = {1, 3};
	game.phase = Phase::Playing;
	assert(game.step() == Sound::Power);
	assert(game.frightenedTicks > 0 && game.score == 50);

	game.reset();
	game.phase = Phase::Playing;
	game.ghosts[0].position = game.player;
	assert(game.step() == Sound::Hit);
	assert(game.lives == 2 && game.phase == Phase::Ready);

	game.reset();
	game.phase = Phase::Playing;
	game.frightenedTicks = 10;
	game.ghosts[0].position = game.player;
	assert(game.step() == Sound::Ghost);
	assert(game.lives == 3 && game.score == 200);

	game.reset();
	game.phase = Phase::Playing;
	game.remaining = 1;
	game.map[game.player.y][game.player.x] = '.';
	assert(game.step() == Sound::Win);
	assert(game.phase == Phase::Won);
	return 0;
}
