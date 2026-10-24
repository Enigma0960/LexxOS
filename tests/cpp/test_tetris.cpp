#include "../../apps/tetris/game.h"

#include <cassert>

using tetris::width;
using tetris::height;
using tetris::PieceKind;
using tetris::Phase;
using tetris::Sound;
using tetris::Game;

namespace {

unsigned countCells(unsigned short mask) {
    unsigned occupied = 0;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            occupied += Game::hasCell(mask, column, row) ? 1U : 0U;
        }
    }
    return occupied;
}

void setO(Game& game, int left, int top) {
    game.kind = PieceKind::O;
    game.shape = Game::shapeFor(PieceKind::O);
    game.x = left;
    game.y = top;
    game.phase = Phase::Playing;
}

} // namespace

int main() {
    Game g{};
    g.reset(123U);
    assert(g.phase == Phase::Ready && g.level == 1 && g.lines == 0 && g.score == 0);
    assert(g.tickIntervalMs() == 700);
    assert(g.gravityStep() == Sound::None && g.y == 0);
    for (unsigned kind = 0; kind < 7; ++kind) {
        assert(countCells(Game::shapeFor(static_cast<PieceKind>(kind))) == 4);
    }

    // Two consecutive 7-bags each contain all seven unique kinds.
    bool seen[7]{};
    unsigned seenCount = 0;
    for (unsigned i = 0; i < 14; ++i) {
        unsigned const index = static_cast<unsigned>(g.kind);
        assert(index < 7);
        if ((i % 7) == 0) {
            for (bool& flag : seen) flag = false;
            seenCount = 0;
        }
        assert(!seen[index]);
        seen[index] = true;
        ++seenCount;
        for (int row = 0; row < height; ++row) {
            for (int column = 0; column < width; ++column) {
                g.board[row][column] = 0;
            }
        }
        g.phase = Phase::Playing;
        (void)g.hardDrop();
        if ((i % 7) == 6) assert(seenCount == 7);
    }

    g.reset(42);
    g.start();
    int const originalX = g.x;
    assert(g.move(-1) && g.x == originalX - 1);
    g.togglePause();
    assert(g.phase == Phase::Paused && !g.move(1));
    assert(g.gravityStep() == Sound::None);
    g.togglePause();
    assert(g.phase == Phase::Playing);

    // Rotation preserves the four cells; squares do not translate.
    for (unsigned kind = 0; kind < 7; ++kind) {
        g.reset(1);
        g.kind = static_cast<PieceKind>(kind);
        g.shape = Game::shapeFor(g.kind);
        g.x = 3;
        g.y = 5;
        g.phase = Phase::Playing;
        assert(g.rotate(true));
        assert(countCells(g.shape) == 4);
        assert(g.rotate(false));
        assert(g.shape == Game::shapeFor(g.kind));
        assert(g.x == 3 && g.y == 5);
    }

    // A wall kick should permit an I rotation close to the right boundary.
    g.reset(2);
    g.kind = PieceKind::I;
    g.shape = Game::shapeFor(g.kind);
    g.x = 6;
    g.y = 5;
    g.phase = Phase::Playing;
    assert(g.rotate(true));
    assert(countCells(g.shape) == 4);

    // Hard drop puts all cells on the bottom, awards 2 points per row.
    g.reset(3);
    setO(g, 3, 0);
    assert(g.landingY() == 17);
    assert(g.hardDrop() == Sound::Lock);
    assert(g.score == 34);
    assert(g.board[18][4] == 2 && g.board[18][5] == 2);
    assert(g.board[19][4] == 2 && g.board[19][5] == 2);
    assert(g.lines == 0);

    // Two adjacent complete rows are cleared together.
    g.reset(4);
    setO(g, 3, 17);
    for (int row = 18; row <= 19; ++row) {
        for (int column = 0; column < width; ++column) {
            g.board[row][column] = (column == 4 || column == 5) ? 0 : 1;
        }
    }
    assert(g.hardDrop() == Sound::Line);
    assert(g.lines == 2 && g.score == 300 && g.level == 1);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            assert(g.board[row][column] == 0);
        }
    }

    // Four rows cleared with a vertical I grant 800 * level points.
    g.reset(5);
    g.kind = PieceKind::I;
    g.shape = 0x4444U; // A vertical line in column 2.
    g.x = 1;
    g.y = 16;
    g.phase = Phase::Playing;
    for (int row = 16; row < 20; ++row) {
        for (int col = 0; col < width; ++col) {
            g.board[row][col] = col == 3 ? 0 : 1;
        }
    }
    assert(g.gravityStep() == Sound::Tetris);
    assert(g.lines == 4 && g.score == 800);

    // Level increases on the tenth cleared line and fall interval decreases.
    g.lines = 9;
    g.level = 1;
    for (int column = 0; column < width; ++column) {
        g.board[19][column] = column == 4 || column == 5 ? 0 : 1;
    }
    setO(g, 3, 17);
    assert(g.hardDrop() == Sound::Line);
    assert(g.lines == 10 && g.level == 2 && g.tickIntervalMs() == 640);
    g.level = 100;
    assert(g.tickIntervalMs() == 100);

    // No space for a new piece is a game-over condition.
    g.reset(6);
    setO(g, 3, 17);
    for (int col = 3; col <= 6; ++col) {
        g.board[1][col] = 1;
    }
    assert(g.hardDrop() == Sound::GameOver && g.phase == Phase::Lost);
    assert(g.softDrop() == Sound::None);

    // Soft drop awards one point per successful cell (not for locking).
    g.reset(7);
    setO(g, 3, 0);
    assert(g.softDrop() == Sound::None && g.y == 1 && g.score == 1);
    return 0;
}
