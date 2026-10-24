#include <lexx/lexx.h>

#include "game.h"

namespace {

constexpr lexx::uint32_t tickMs = 170;
constexpr int frameCapacity = 4096;
char frame[frameCapacity]{};
lexx::size_t frameSize = 0;

pacman::Game game{};

bool soundEnabled = true; // User-controlled mute only; do not auto-mute on a failed request.
bool lastSoundFailed = false;
unsigned soundRequests = 0;
unsigned soundFailures = 0;
unsigned keyEscapeState = 0;

void append(char const* text) {
	while (*text != '\0' && frameSize < sizeof(frame)) {
		frame[frameSize++] = *text++;
	}
}

void put(char c) {
	if (frameSize < sizeof(frame)) {
		frame[frameSize++] = c;
	}
}

void number(unsigned n) {
	char digits[11]{};
	unsigned used = 0;
	do {
		digits[used++] = static_cast<char>('0' + n % 10U);
		n /= 10U;
	} while (n && used < sizeof(digits));
	while (used) {
		put(digits[--used]);
	}
}

void color(char tile) {
	switch (tile) {
		case '#': append("\x1b[1;34m"); break;
		case '.': append("\x1b[90m"); break;
		case 'o': append("\x1b[1;37m"); break;
		case '@': append("\x1b[1;33m"); break;
		case 'M': append("\x1b[1;31m"); break;
		case 'm': append("\x1b[1;36m"); break;
		default : append("\x1b[0m"); break;
	}
}

void draw() {
	frameSize = 0;
	append("\x1b[H\x1b[1;33m LEXX PAC-MAN \x1b[0m    SCORE ");
	number(game.score);
	append("  LIVES ");
	number(game.lives);
	append("\x1b[K\r\n");
	append("  ");
	for (int x = 0; x < pacman::width; ++x) {
		put('=');
	}
	append("\x1b[K\r\n");

	for (int y = 0; y < pacman::height; ++y) {
		append("  ");
		char previous = '\0';
		for (int x = 0; x < pacman::width; ++x) {
			char tile = game.map[y][x];
			for (pacman::Ghost const& ghost : game.ghosts) {
				if (ghost.position.x == x && ghost.position.y == y) {
					tile = game.frightenedTicks != 0 ? 'm' : 'M';
				}
			}
			if (game.player.x == x && game.player.y == y) {
				tile = '@';
			}
			if (tile != previous) {
				color(tile);
				previous = tile;
			}
			put(tile);
		}
		append("\x1b[0m\x1b[K\r\n");
	}
	append("  ");
	for (int x = 0; x < pacman::width; ++x) {
		put('=');
	}
	append("\x1b[K\r\n");
	append("  ");
	switch (game.phase) {
		case pacman::Phase::Ready: append("ENTER / ARROWS to start"); break;
		case pacman::Phase::Playing:
			append("Dots left: ");
			number(game.remaining);
			break;
		case pacman::Phase::Paused: append("PAUSED - press P"); break;
		case pacman::Phase::Won   : append("YOU WIN! R to restart"); break;
		case pacman::Phase::Lost  : append("GAME OVER - R to restart"); break;
	}

	append("\x1b[K\r\n  Arrows/WASD  P pause  M mute");
	append("\x1b[K\r\n  R reset   Q quit");
	append("\x1b[K\x1b[0m");
	(void) lexx::write(frame, frameSize);
}

// Keep audio errors visible instead of silently muting the rest of the game.
// A successful syscall means that AudioDevice accepted the tone request;
// the host speaker still depends on the emulator's audio-output settings.
bool playTone(lexx::uint32_t frequency, lexx::uint32_t duration, lexx::uint8_t volume) {
	++soundRequests;
	bool const ok = lexx::play_tone(frequency, duration, volume);
	lastSoundFailed = !ok;
	if (!ok) {
		++soundFailures;
	}
	return ok;
}

void sound(pacman::Sound event) {
	if (!soundEnabled || event == pacman::Sound::None) {
		return;
	}
	lexx::uint32_t hz = 640;
	lexx::uint32_t ms = 90;
	lexx::uint8_t volume = 180;
	switch (event) {
		case pacman::Sound::Dot:
			hz = 640;
			ms = 90;
			break;
		case pacman::Sound::Power:
			hz = 440;
			ms = 170;
			break;
		case pacman::Sound::Ghost:
			hz = 1000;
			ms = 170;
			break;
		case pacman::Sound::Hit:
			hz = 180;
			ms = 280;
			volume = 200;
			break;
		case pacman::Sound::Win:
			hz = 1200;
			ms = 340;
			volume = 200;
			break;
		case pacman::Sound::None:
			return;
	}
	(void) playTone(hz, ms, volume);
}

bool handleKey(lexx::KeyEvent const& key, bool& quitting) {
	if (key.type != static_cast<lexx::uint8_t>(lexx::KeyEventType::KeyDown)) {
		return false;
	}
	pacman::Direction direction = pacman::Direction::None;
	switch (key.key_code) {
		case 1 : direction = pacman::Direction::Up; break;
		case 2 : direction = pacman::Direction::Down; break;
		case 3 : direction = pacman::Direction::Left; break;
		case 4 : direction = pacman::Direction::Right; break;
		case 6 : quitting = true; return false; // hardware Escape
		default: break;
	}
	if (direction == pacman::Direction::None) {
		char c = static_cast<char>(key.character);
		// UART arrows arrive as ESC [ A/B/C/D when physical event codes are unavailable.
		if (keyEscapeState == 1) {
			keyEscapeState = c == '[' ? 2U : 0U;
			return false;
		}
		if (keyEscapeState == 2) {
			keyEscapeState = 0;
			switch (c) {
				case 'A': direction = pacman::Direction::Up; break;
				case 'B': direction = pacman::Direction::Down; break;
				case 'C': direction = pacman::Direction::Right; break;
				case 'D': direction = pacman::Direction::Left; break;
				default : break;
			}
		} else if (c == '\x1b') {
			keyEscapeState = 1;
			return false;
		} else {
			switch (c) {
				case 'w':
				case 'W' : direction = pacman::Direction::Up; break;
				case 's' :
				case 'S' : direction = pacman::Direction::Down; break;
				case 'a' :
				case 'A' : direction = pacman::Direction::Left; break;
				case 'd' :
				case 'D' : direction = pacman::Direction::Right; break;
				case 'q' :
				case 'Q' : quitting = true; return false;
				case 'm' :
				case 'M' : soundEnabled = !soundEnabled; return true;
				case 't' :
				case 'T' : (void) playTone(880, 300, 200); return true;
				case 'p' :
				case 'P' :
				case ' ' : game.togglePause(); return true;
				case 'r' :
				case 'R' : game.reset(); return true;
				case '\r':
				case '\n': game.start(); return true;
				default  : break;
			}
		}
	}
	if (direction != pacman::Direction::None) {
		game.turn(direction);
		return true;
	}
	return false;
}

int gameMain() {
	game.reset();
	lexx::write("\x1b[2J\x1b[H\x1b[?25l");
	draw();
	bool quitting = false;
	lexx::uint32_t lastTick = lexx::milliseconds();
	if (lastTick == lexx::no_data) {
		lexx::write("\x1b[0m\x1b[?25h\x1b[2J\x1b[Hpacman: clock unavailable\r\n");
		return 1;
	}
	while (!quitting) {
		bool redraw = false;
		lexx::KeyEvent event{};
		// Drain a bounded batch so a key flood cannot starve the game loop.
		for (unsigned count = 0; count < 24 && lexx::poll_key(event); ++count) {
			redraw = handleKey(event, quitting) || redraw;
			if (quitting) {
				break;
			}
		}
		if (quitting) {
			break;
		}
		lexx::uint32_t const now = lexx::milliseconds();
		if (static_cast<lexx::uint32_t>(now - lastTick) >= tickMs) {
			lastTick = now;
			sound(game.step());
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
