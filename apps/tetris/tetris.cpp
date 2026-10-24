#include <lexx/lexx.h>

#include "game.h"

namespace {

constexpr int frameCapacity = 8192;
char frame[frameCapacity]{};
unsigned char tiles[tetris::height][tetris::width]{};
lexx::size_t frameSize = 0;
tetris::Game game{};
unsigned bestScore = 0;
bool soundEnabled = true;
bool lastSoundFailed = false;
unsigned soundRequests = 0;
unsigned soundFailures = 0;
unsigned escapeState = 0;

void append(char const* str) {
	while (*str != '\0' && frameSize < sizeof(frame)) {
		frame[frameSize++] = *str++;
	}
}

void put(char c) {
	if (frameSize < sizeof(frame)) {
		frame[frameSize++] = c;
	}
}

void number(unsigned value) {
	char digits[11]{};
	unsigned count = 0;
	do {
		digits[count++] = static_cast<char>('0' + value % 10U);
		value /= 10U;
	} while (value != 0 && count < sizeof(digits));
	while (count != 0) {
		put(digits[--count]);
	}
}

void tileColor(unsigned char color) {
	switch (color) {
		case 1 : append("\x1b[1;36m"); break; // I
		case 2 : append("\x1b[1;33m"); break; // O
		case 3 : append("\x1b[1;35m"); break; // T
		case 4 : append("\x1b[1;32m"); break; // S
		case 5 : append("\x1b[1;31m"); break; // Z
		case 6 : append("\x1b[1;34m"); break; // J
		case 7 : append("\x1b[1;37m"); break; // L
		case 8 : append("\x1b[90m"); break; // Ghost
		default: append("\x1b[0m"); break;
	}
}

void draw() {
	frameSize = 0;
	append("\x1b[H\x1b[1;36m LEXX TETRIS\x1b[0m   SCORE:");
	number(game.score);
	append("  BEST:");
	number(bestScore);
	append("\x1b[K\r\n  +--------------------+\x1b[K\r\n");

	// Build a compact tile grid once instead of probing the VM state for
	// each console glyph. Existing blocks, projected landing and live piece.
	for (int row = 0; row < tetris::height; ++row) {
		for (int column = 0; column < tetris::width; ++column) {
			tiles[row][column] = game.board[row][column];
		}
	}
	if (game.phase != tetris::Phase::Lost) {
		int const landing = game.landingY();
		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				if (!tetris::Game::hasCell(game.shape, column, row)) {
					continue;
				}
				int const shadowY = landing + row;
				int const col = game.x + column;
				if (shadowY >= 0 && shadowY < tetris::height && col >= 0 && col < tetris::width && tiles[shadowY][col] == 0) {
					tiles[shadowY][col] = 8;
				}
				int const actualY = game.y + row;
				if (actualY >= 0 && actualY < tetris::height && col >= 0 && col < tetris::width) {
					tiles[actualY][col] = static_cast<unsigned char>(static_cast<unsigned>(game.kind) + 1U);
				}
			}
		}
	}

	for (int row = 0; row < tetris::height; ++row) {
		append("  |\x1b[0m");
		unsigned char previous = 255;
		for (int column = 0; column < tetris::width; ++column) {
			unsigned char const tile = tiles[row][column];
			if (tile != previous) {
				tileColor(tile);
				previous = tile;
			}
			append(tile == 0 ? "  " : tile == 8 ? ".."
			                                    : "[]");
		}
		append("\x1b[0m|\x1b[K");
		if (row == 0) {
			append("  NEXT:");
		} else if (row >= 1 && row <= 4) {
			unsigned short const mask = tetris::Game::shapeFor(game.next);
			append("  ");
			tileColor(static_cast<unsigned char>(static_cast<unsigned>(game.next) + 1U));
			for (int col = 0; col < 4; ++col) {
				append(tetris::Game::hasCell(mask, col, row - 1) ? "[]" : "  ");
			}
			append("\x1b[0m");
		} else if (row == 6) {
			append("  LINES:");
			number(game.lines);
		} else if (row == 7) {
			append("  LEVEL:");
			number(game.level);
		} else if (row == 8) {
			append("  FALL:");
			number(game.tickIntervalMs());
			append("ms");
		}
		append("\r\n");
	}
	append("  +--------------------+\x1b[K\r\n  ");
	switch (game.phase) {
		case tetris::Phase::Ready  : append("ENTER to start"); break;
		case tetris::Phase::Playing: append("CLEAR LINES TO LEVEL UP"); break;
		case tetris::Phase::Paused : append("PAUSED - press P"); break;
		case tetris::Phase::Lost   : append("GAME OVER - press R"); break;
	}
	append("\x1b[K\r\n  LEFT/RIGHT move  UP rotate  Z reverse");
	append("\x1b[K\r\n  DOWN soft SPACE drop P pause R reset");
	append("\x1b[K\r\n  M mute Q quit");
	append("\x1b[K\r\n  SFX:");
	append(soundEnabled ? "ON" : "OFF");
	append(" DEVICE:");
	append(lastSoundFailed ? "ERROR" : soundRequests ? "OK"
	                                                 : "UNTESTED");
	append(" TX:");
	number(soundRequests);
	append(" ERR:");
	number(soundFailures);
	append("\x1b[K\x1b[0m");
	(void) lexx::write(frame, frameSize);
}

void tone(lexx::uint32_t hz, lexx::uint32_t duration, lexx::uint8_t volume) {
	++soundRequests;
	bool const accepted = lexx::play_tone(hz, duration, volume);
	lastSoundFailed = !accepted;
	if (!accepted) {
		++soundFailures;
	}
}

void sound(tetris::Sound effect) {
	if (!soundEnabled) {
		return;
	}
	switch (effect) {
		case tetris::Sound::Rotate  : tone(760, 65, 125); break;
		case tetris::Sound::Lock    : tone(380, 65, 125); break;
		case tetris::Sound::Line    : tone(1050, 170, 200); break;
		case tetris::Sound::Tetris  : tone(1350, 320, 210); break;
		case tetris::Sound::GameOver: tone(180, 360, 210); break;
		case tetris::Sound::None    : break;
	}
}

enum class Action : unsigned char {
	None,
	Left,
	Right,
	Down,
	Clockwise,
	Counterclockwise
};

bool applyAction(Action action, lexx::uint32_t& lastFall) {
	bool changed = false;
	switch (action) {
		case Action::Left : changed = game.move(-1); break;
		case Action::Right: changed = game.move(1); break;
		case Action::Down:
			if (game.phase == tetris::Phase::Playing) {
				sound(game.softDrop());
				lastFall = lexx::milliseconds();
				changed = true;
			}
			break;
		case Action::Clockwise:
		case Action::Counterclockwise:
			changed = game.rotate(action == Action::Clockwise);
			if (changed) {
				sound(tetris::Sound::Rotate);
			}
			break;
		case Action::None: break;
	}
	return changed;
}

bool handleKey(lexx::KeyEvent const& key, bool& quitting, lexx::uint32_t& lastFall) {
	if (key.type != static_cast<lexx::uint8_t>(lexx::KeyEventType::KeyDown)) {
		return false;
	}
	Action action = Action::None;
	switch (key.key_code) {
		case 1 : action = Action::Clockwise; break;
		case 2 : action = Action::Down; break;
		case 3 : action = Action::Left; break;
		case 4 : action = Action::Right; break;
		case 6 : quitting = true; return false;
		default: break;
	}
	if (action == Action::None) {
		char const c = static_cast<char>(key.character);
		// UART arrow escape sequences, in addition to direct key events.
		if (escapeState == 1) {
			escapeState = (c == '[' || c == 'O') ? 2U : 0U;
			return false;
		}
		if (escapeState == 2) {
			escapeState = 0;
			switch (c) {
				case 'A': action = Action::Clockwise; break;
				case 'B': action = Action::Down; break;
				case 'C': action = Action::Right; break;
				case 'D': action = Action::Left; break;
				default : break;
			}
		} else if (c == '\x1b') {
			escapeState = 1;
			return false;
		} else {
			switch (c) {
				case 'a':
				case 'A': action = Action::Left; break;
				case 'd':
				case 'D': action = Action::Right; break;
				case 'w':
				case 'W':
				case 'x':
				case 'X': action = Action::Clockwise; break;
				case 'z':
				case 'Z': action = Action::Counterclockwise; break;
				case 's':
				case 'S': action = Action::Down; break;
				case ' ': // Hard drop is deliberately different from Snake's pause key.
					if (game.phase == tetris::Phase::Playing) {
						sound(game.hardDrop());
						lastFall = lexx::milliseconds();
						return true;
					}
					return false;
				case 'q':
				case 'Q': quitting = true; return false;
				case 'p':
				case 'P':
					game.togglePause();
					lastFall = lexx::milliseconds();
					return true;
				case 'r':
				case 'R':
					game.reset(lexx::milliseconds());
					lastFall = lexx::milliseconds();
					return true;
				case 'm':
				case 'M': soundEnabled = !soundEnabled; return true;
				case 't':
				case 'T': tone(880, 300, 200); return true;
				case '\r':
				case '\n':
					game.start();
					lastFall = lexx::milliseconds();
					return true;
				default: break;
			}
		}
	}
	if (action != Action::None) {
		if (game.phase == tetris::Phase::Ready) {
			game.start();
			lastFall = lexx::milliseconds();
		}
		return applyAction(action, lastFall);
	}
	return false;
}

int gameMain() {
	lexx::uint32_t lastFall = lexx::milliseconds();
	if (lastFall == lexx::no_data) {
		lexx::write("tetris: clock unavailable\r\n");
		return 1;
	}
	game.reset(lastFall);
	lexx::write("\x1b[2J\x1b[H\x1b[?25l");
	draw();
	bool quitting = false;
	while (!quitting) {
		bool redraw = false;
		lexx::KeyEvent key{};
		for (unsigned count = 0; count < 24 && lexx::poll_key(key); ++count) {
			redraw = handleKey(key, quitting, lastFall) || redraw;
			if (quitting) {
				break;
			}
		}
		if (quitting) {
			break;
		}
		lexx::uint32_t const now = lexx::milliseconds();
		if (game.phase == tetris::Phase::Playing) {
			if (static_cast<lexx::uint32_t>(now - lastFall) >= game.tickIntervalMs()) {
				lastFall = now;
				sound(game.gravityStep());
				redraw = true;
			}
		} else {
			lastFall = now; // Avoid catch-up after pause, ready or game over.
		}
		if (game.score > bestScore) {
			bestScore = game.score;
			redraw = true;
		}
		if (redraw) {
			draw();
		}
		lexx::yield();
	}
	lexx::write("\x1b[0m\x1b[?25h\x1b[2J\x1b[H");
	return 0;
}

} // namespace

extern "C" [[noreturn]] void _start() {
	lexx::exit(gameMain());
}
