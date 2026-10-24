#include "game.h"

namespace tetris {
namespace {
	// Four rows of four bits. The empty top row is the piece spawn margin.
	constexpr unsigned short shapes[7] = {
	    0x00F0U, // I  .... / #### / .... / ....
	    0x0660U, // O  .... / .##. / .##. / ....
	    0x0270U, // T  .... / ###. / .#.. / ....
	    0x0360U, // S  .... / .##. / ##.. / ....
	    0x0630U, // Z  .... / ##.. / .##. / ....
	    0x0710U, // J  .... / #... / ###. / ....
	    0x0740U, // L  .... / ..#. / ###. / ....
	};

	unsigned short rotated(unsigned short source, bool clockwise) {
		unsigned short result = 0;
		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				if ((source & (1U << (row * 4 + column))) == 0) {
					continue;
				}
				int const targetX = clockwise ? 3 - row : row;
				int const targetY = clockwise ? column : 3 - column;
				result = static_cast<unsigned short>(result | (1U << (targetY * 4 + targetX)));
			}
		}
		return result;
	}
} // namespace

unsigned short Game::shapeFor(PieceKind piece) {
	return shapes[static_cast<unsigned>(piece)];
}

bool Game::hasCell(unsigned short mask, int cellX, int cellY) {
	return cellX >= 0 && cellX < 4 && cellY >= 0 && cellY < 4 && (mask & (1U << (cellY * 4 + cellX))) != 0;
}

unsigned Game::random() {
	randomState ^= randomState << 13U;
	randomState ^= randomState >> 17U;
	randomState ^= randomState << 5U;
	return randomState;
}

PieceKind Game::drawFromBag() {
	if (bagIndex >= 7) {
		for (unsigned i = 0; i < 7; ++i) {
			bag[i] = static_cast<PieceKind>(i);
		}
		for (unsigned i = 6; i > 0; --i) {
			unsigned const choice = random() % (i + 1U);
			PieceKind const temp = bag[i];
			bag[i] = bag[choice];
			bag[choice] = temp;
		}
		bagIndex = 0;
	}
	return bag[bagIndex++];
}

void Game::spawnPiece() {
	kind = next;
	next = drawFromBag();
	shape = shapeFor(kind);
	x = 3;
	y = 0;
	if (!canPlace(shape, x, y)) {
		phase = Phase::Lost;
	}
}

void Game::reset(unsigned seed) {
	for (int row = 0; row < height; ++row) {
		for (int column = 0; column < width; ++column) {
			board[row][column] = 0;
		}
	}
	score = 0;
	lines = 0;
	level = 1;
	phase = Phase::Ready;
	randomState = seed != 0 ? seed : 0x9E3779B9U;
	bagIndex = 7;
	next = drawFromBag();
	spawnPiece();
}

void Game::start() {
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

bool Game::canPlace(unsigned short mask, int left, int top) const {
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			if (!hasCell(mask, column, row)) {
				continue;
			}
			int const boardX = left + column;
			int const boardY = top + row;
			if (boardX < 0 || boardX >= width || boardY >= height) {
				return false;
			}
			if (boardY >= 0 && board[boardY][boardX] != 0) {
				return false;
			}
		}
	}
	return true;
}

bool Game::move(int deltaX) {
	if (phase != Phase::Playing || (deltaX != -1 && deltaX != 1)) {
		return false;
	}
	if (!canPlace(shape, x + deltaX, y)) {
		return false;
	}
	x += deltaX;
	return true;
}

bool Game::rotate(bool clockwise) {
	if (phase != Phase::Playing) {
		return false;
	}
	if (kind == PieceKind::O) {
		return true; // Do not shift a square when rotating.
	}
	unsigned short const candidate = rotated(shape, clockwise);
	// Small wall/floor kicks: not full SRS, but allows turns at borders.
	constexpr int kicksX[] = {0, -1, 1, -2, 2, 0, -1, 1, 0};
	constexpr int kicksY[] = {0, 0, 0, 0, 0, -1, -1, -1, -2};
	for (unsigned i = 0; i < sizeof(kicksX) / sizeof(kicksX[0]); ++i) {
		int const targetX = x + kicksX[i];
		int const targetY = y + kicksY[i];
		if (canPlace(candidate, targetX, targetY)) {
			x = targetX;
			y = targetY;
			shape = candidate;
			return true;
		}
	}
	return false;
}

Sound Game::lockPiece() {
	// Lock a piece, even if one of its blocks is above the board.
	bool topOut = false;
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			if (!hasCell(shape, column, row)) {
				continue;
			}
			int const yy = y + row;
			if (yy < 0) {
				topOut = true;
			} else {
				board[yy][x + column] = static_cast<unsigned char>(static_cast<unsigned>(kind) + 1U);
			}
		}
	}
	if (topOut) {
		phase = Phase::Lost;
		return Sound::GameOver;
	}

	unsigned cleared = 0;
	for (int row = height - 1; row >= 0; --row) {
		bool full = true;
		for (int column = 0; column < width; ++column) {
			if (board[row][column] == 0) {
				full = false;
				break;
			}
		}
		if (!full) {
			continue;
		}
		++cleared;
		for (int dest = row; dest > 0; --dest) {
			for (int column = 0; column < width; ++column) {
				board[dest][column] = board[dest - 1][column];
			}
		}
		for (int column = 0; column < width; ++column) {
			board[0][column] = 0;
		}
		++row; // Inspect the row shifted into this position as well.
	}

	if (cleared != 0) {
		constexpr unsigned bonuses[] = {0, 100, 300, 500, 800};
		score += bonuses[cleared] * level;
		lines += cleared;
		level = 1U + lines / 10U;
	}
	spawnPiece();
	if (phase == Phase::Lost) {
		return Sound::GameOver;
	}
	return cleared == 4 ? Sound::Tetris : (cleared != 0 ? Sound::Line : Sound::Lock);
}

Sound Game::softDrop() {
	if (phase != Phase::Playing) {
		return Sound::None;
	}
	if (canPlace(shape, x, y + 1)) {
		++y;
		++score; // One point per manually descended row.
		return Sound::None;
	}
	return lockPiece();
}

Sound Game::hardDrop() {
	if (phase != Phase::Playing) {
		return Sound::None;
	}
	unsigned distance = 0;
	while (canPlace(shape, x, y + 1)) {
		++y;
		++distance;
	}
	score += 2U * distance;
	return lockPiece();
}

Sound Game::gravityStep() {
	if (phase != Phase::Playing) {
		return Sound::None;
	}
	if (canPlace(shape, x, y + 1)) {
		++y;
		return Sound::None;
	}
	return lockPiece();
}

int Game::landingY() const {
	int landing = y;
	while (canPlace(shape, x, landing + 1)) {
		++landing;
	}
	return landing;
}

unsigned Game::tickIntervalMs() const {
	unsigned const decrease = (level - 1U) * 60U;
	return decrease >= 600U ? 100U : 700U - decrease;
}

} // namespace tetris
