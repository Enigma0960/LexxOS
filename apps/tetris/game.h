#pragma once

namespace tetris {

constexpr int width = 10;
constexpr int height = 20;

enum class PieceKind : unsigned char {
	I,
	O,
	T,
	S,
	Z,
	J,
	L
};
enum class Phase : unsigned char {
	Ready,
	Playing,
	Paused,
	Lost
};
enum class Sound : unsigned char {
	None,
	Rotate,
	Lock,
	Line,
	Tetris,
	GameOver
};

struct Game {
	unsigned char board[height][width]{}; // Zero means empty; 1..7 are piece colors.
	PieceKind kind{PieceKind::I};
	PieceKind next{PieceKind::I};
	unsigned short shape{0}; // Occupied cells in a 4x4 bitmap.
	int x{3};
	int y{0};
	Phase phase{Phase::Ready};
	unsigned score{0};
	unsigned lines{0};
	unsigned level{1};
	unsigned randomState{0};
	PieceKind bag[7]{};
	unsigned bagIndex{7};

	void reset(unsigned seed);
	void start();
	void togglePause();
	bool move(int deltaX);
	bool rotate(bool clockwise);
	Sound softDrop();
	Sound hardDrop();
	Sound gravityStep();

	[[nodiscard]] unsigned tickIntervalMs() const;
	[[nodiscard]] int landingY() const;
	[[nodiscard]] static unsigned short shapeFor(PieceKind piece);
	[[nodiscard]] static bool hasCell(unsigned short mask, int cellX, int cellY);

private:
	[[nodiscard]] unsigned random();
	[[nodiscard]] PieceKind drawFromBag();
	[[nodiscard]] bool canPlace(unsigned short mask, int left, int top) const;
	void spawnPiece();
	Sound lockPiece();
};

} // namespace tetris
