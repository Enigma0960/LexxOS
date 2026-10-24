#pragma once

/**
 * Compile-time feature checks for LexxOS.
 *
 * Every feature must be generated into feature_list.h with one of these values:
 * -  1: enabled;
 * - -1: disabled.
 *
 * Undefined or zero-valued features intentionally fail compilation when checked.
 */
#define FEATURE_ENABLED(feature) (1 / FEATURE_##feature == 1)
#define FEATURE_DISABLED(feature) (1 / FEATURE_##feature == -1)

#include <feature_list.h>
