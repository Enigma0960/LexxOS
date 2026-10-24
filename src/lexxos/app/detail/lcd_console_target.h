#pragma once

#include <lexxos/board_config.h>
#include <lexxos/feature.h>

#define LEXXOS_LCD_CONSOLE_TARGET_ENABLED \
	(FEATURE_ENABLED(DISPLAY)             \
	    && (BOARD_FAMILY_ENABLED(PICOCALC) || BOARD_FAMILY_ENABLED(CARDPUTER) || BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV)))
