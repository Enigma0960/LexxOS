#pragma once

/**
 * Compile-time board checks for LexxOS.
 *
 * Board selectors live in a separate namespace from feature selectors:
 * - LEXXOS_BOARD_<BOARD_ID>: exact active board;
 * - LEXXOS_BOARD_FAMILY_<FAMILY>: board-family grouping.
 *
 * Undefined or zero-valued board flags intentionally fail compilation when
 * checked through these macros.
 */

#include <board_list.h>

#define BOARD_ENABLED(board) (1 / LEXXOS_BOARD_##board == 1)
#define BOARD_DISABLED(board) (1 / LEXXOS_BOARD_##board == -1)
#define BOARD_FAMILY_ENABLED(family) (1 / LEXXOS_BOARD_FAMILY_##family == 1)
#define BOARD_FAMILY_DISABLED(family) (1 / LEXXOS_BOARD_FAMILY_##family == -1)
