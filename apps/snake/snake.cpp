#include <lexx/lexx.h>

#include "game.h"

namespace {

constexpr int frameCapacity = 4096;
char frame[frameCapacity]{};
char tiles[snake::height][snake::width]{};
lexx::size_t frameSize = 0;
snake::Game game{};
unsigned bestScore = 0;
bool soundEnabled = true;
bool soundFailed = false;
unsigned soundRequests = 0;
unsigned soundFailures = 0;
unsigned escapeState = 0;

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

void number(unsigned value) {
    char digits[11]{};
    unsigned used = 0;
    do {
        digits[used++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    } while (value != 0 && used < sizeof(digits));
    while (used != 0) {
        put(digits[--used]);
    }
}

void color(char tile) {
    switch (tile) {
        case '#': append("\x1b[1;34m"); break;
        case '@': append("\x1b[1;32m"); break;
        case 'o': append("\x1b[32m"); break;
        case '*': append("\x1b[1;31m"); break;
        default: append("\x1b[0m"); break;
    }
}

void draw() {
    frameSize = 0;
    append("\x1b[H\x1b[1;32m LEXX SNAKE\x1b[0m  SCORE:");
    number(game.score);
    append(" BEST:");
    number(bestScore);
    append("\x1b[K\r\n");

    // Build a tile grid once per frame. Avoid testing every segment for every
    // character: it becomes expensive on the interpreted RV32I guest CPU.
    for (int y = 0; y < snake::height; ++y) {
        for (int x = 0; x < snake::width; ++x) {
            tiles[y][x] = (x == 0 || y == 0 || x == snake::width - 1 || y == snake::height - 1)
                          ? '#' : ' ';
        }
    }
    tiles[game.food.y][game.food.x] = '*';
    for (unsigned i = 0; i < game.length; ++i) {
        tiles[game.body[i].y][game.body[i].x] = i == 0 ? '@' : 'o';
    }
    for (int y = 0; y < snake::height; ++y) {
        append("  ");
        char previous = '\0';
        for (int x = 0; x < snake::width; ++x) {
            char const tile = tiles[y][x];
            if (tile != previous) {
                color(tile);
                previous = tile;
            }
            put(tile);
        }
        append("\x1b[0m\x1b[K\r\n");
    }
    append("  ");
    switch (game.phase) {
        case snake::Phase::Ready: append("ENTER / ARROWS to start"); break;
        case snake::Phase::Playing:
            append("LENGTH:"); number(game.length);
            append("  SPEED:"); number(game.tickIntervalMs()); append("ms");
            break;
        case snake::Phase::Paused: append("PAUSED - press P to resume"); break;
        case snake::Phase::Won: append("YOU WIN! Press R to restart"); break;
        case snake::Phase::Lost: append("GAME OVER! Press R to restart"); break;
    }

    append("\x1b[K\r\n  Arrows/WASD  P pause  M mute");
    append("\x1b[K\r\n  R reset   Q quit");
    append("\x1b[K\x1b[0m");

    (void) lexx::write(frame, frameSize);
}

void tone(lexx::uint32_t hz, lexx::uint32_t duration, lexx::uint8_t volume) {
    ++soundRequests;
    bool const success = lexx::play_tone(hz, duration, volume);
    soundFailed = !success;
    if (!success) {
        ++soundFailures;
    }
}

void sound(snake::Sound effect) {
    if (!soundEnabled) {
        return;
    }
    switch (effect) {
        case snake::Sound::Eat:   tone(850, 100, 190); break;
        case snake::Sound::Crash: tone(210, 260, 200); break;
        case snake::Sound::Win:   tone(1200, 400, 200); break;
        case snake::Sound::None:  break;
    }
}

bool handleKey(lexx::KeyEvent const& key, bool& quitting) {
    if (key.type != static_cast<lexx::uint8_t>(lexx::KeyEventType::KeyDown)) {
        return false;
    }
    snake::Direction direction = snake::Direction::Right;
    bool hasDirection = true;
    switch (key.key_code) {
        case 1: direction = snake::Direction::Up; break;
        case 2: direction = snake::Direction::Down; break;
        case 3: direction = snake::Direction::Left; break;
        case 4: direction = snake::Direction::Right; break;
        case 6: quitting = true; return false; // Hardware Escape.
        default: hasDirection = false; break;
    }
    if (!hasDirection) {
        char const c = static_cast<char>(key.character);
        if (escapeState == 1) {
            escapeState = c == '[' ? 2U : 0U;
            return false;
        }
        if (escapeState == 2) {
            escapeState = 0;
            hasDirection = true;
            switch (c) {
                case 'A': direction = snake::Direction::Up; break;
                case 'B': direction = snake::Direction::Down; break;
                case 'C': direction = snake::Direction::Right; break;
                case 'D': direction = snake::Direction::Left; break;
                default: hasDirection = false; break;
            }
        } else if (c == '\x1b') {
            escapeState = 1;
            return false;
        } else {
            hasDirection = true;
            switch (c) {
                case 'w': case 'W': direction = snake::Direction::Up; break;
                case 's': case 'S': direction = snake::Direction::Down; break;
                case 'a': case 'A': direction = snake::Direction::Left; break;
                case 'd': case 'D': direction = snake::Direction::Right; break;
                case 'q': case 'Q': quitting = true; return false;
                case 'm': case 'M': soundEnabled = !soundEnabled; return true;
                case 't': case 'T': tone(880, 300, 200); return true;
                case 'p': case 'P': case ' ': game.togglePause(); return true;
                case 'r': case 'R': game.reset(lexx::milliseconds()); return true;
                case '\r': case '\n': game.start(); return true;
                default: hasDirection = false; break;
            }
        }
    }
    if (hasDirection) {
        game.turn(direction);
        return true;
    }
    return false;
}

int gameMain() {
    lexx::uint32_t lastTick = lexx::milliseconds();
    if (lastTick == lexx::no_data) {
        lexx::write("snake: clock unavailable\r\n");
        return 1;
    }
    game.reset(lastTick);
    lexx::write("\x1b[2J\x1b[H\x1b[?25l");
    draw();
    bool quitting = false;
    while (!quitting) {
        bool redraw = false;
        lexx::KeyEvent event{};
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
        if (game.phase == snake::Phase::Playing) {
            if (static_cast<lexx::uint32_t>(now - lastTick) >= game.tickIntervalMs()) {
                lastTick = now;
                sound(game.step());
                if (game.score > bestScore) {
                    bestScore = game.score;
                }
                redraw = true;
            }
        } else {
            // Don't accumulate a time debt while ready or paused.
            lastTick = now;
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
