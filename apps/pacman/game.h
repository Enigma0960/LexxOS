#pragma once

namespace pacman {

constexpr int width = 25;
constexpr int height = 15;

enum class Direction : unsigned char { None, Up, Down, Left, Right };
enum class Phase : unsigned char { Ready, Playing, Paused, Won, Lost };
enum class Sound : unsigned char { None, Dot, Power, Ghost, Hit, Win };

struct Point {
    int x{0};
    int y{0};
};

struct Ghost {
    Point position{};
    Direction direction{Direction::None};
    Point home{};
};

struct Game {
    char map[height][width + 1]{};
    Point player{};
    Ghost ghosts[2]{};
    Direction direction{Direction::None};
    Direction requested{Direction::None};
    Phase phase{Phase::Ready};
    unsigned score{0};
    unsigned remaining{0};
    unsigned lives{3};
    unsigned frightenedTicks{0};
    unsigned ticks{0};
    unsigned randomState{0x9E3779B9U};

    void reset();
    void turn(Direction value);
    void togglePause();
    void start();
    Sound step();
    bool walkable(Point position) const;

private:
    void resetActors();
    Sound collide();
    void moveGhost(Ghost& ghost);
    unsigned random();
};

} // namespace pacman
