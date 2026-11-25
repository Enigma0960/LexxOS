#!/bin/sh
# Host MMIO model: no hardware accesses.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT HUP INT TERM
"${CC:-cc}" -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter \
    -fsanitize=address,undefined -I"$here/include" \
    "$here/test_splash.c" -o "$output/test_splash"
for scenario in success timeout drain missing clock; do
    "$output/test_splash" "$scenario"
done
