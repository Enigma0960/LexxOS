#pragma once

namespace snake {

// The enclosing walls are part of the board. Playable cells are inside them.
constexpr int width = 28;
constexpr int height = 15;
constexpr int maxLength = (width - 2) * (height - 2);

enum class Direction : unsigned char {
	Up,
	Down,
	Left,
	Right
};
enum class Phase : unsigned char {
	Ready,
	Playing,
	Paused,
	Won,
	Lost
};
enum class Sound : unsigned char {
	None,
	Eat,
	Crash,
	Win
};

struct Point {
	int x{0};
	int y{0};
};

struct Game {
	Point body[maxLength]{}; // Head at index 0.
	Point food{};
	Direction direction{Direction::Right};
	Direction pending{Direction::Right};
	Phase phase{Phase::Ready};
	unsigned length{3};
	unsigned score{0};
	unsigned eaten{0};
	unsigned randomState{0xA341316CU};
	bool turnQueued{false};

	void reset(unsigned seed = 0xA341316CU);
	void start();
	void turn(Direction next);
	void togglePause();
	Sound step();
	[[nodiscard]] bool occupies(Point p) const;
	[[nodiscard]] unsigned tickIntervalMs() const;

private:
	[[nodiscard]] unsigned random();
	[[nodiscard]] bool placeFood();
};

} // namespace snake
