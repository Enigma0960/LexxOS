#!/usr/bin/env bash
set -euo pipefail

UF2="${1:-build/minimal-picocalc_pico_w/src/lexxos.uf2}"
BOOTSEL_VOLUME="${2:-${PICO_BOOTSEL_VOLUME:-}}"

if [[ -z "${BOOTSEL_VOLUME}" ]]; then
	echo "Pass the mounted BOOTSEL volume as the second argument or set PICO_BOOTSEL_VOLUME." >&2
	exit 2
fi

if [[ ! -f "${UF2}" ]]; then
	echo "UF2 file not found: ${UF2}" >&2
	exit 1
fi

if [[ ! -d "${BOOTSEL_VOLUME}" ]]; then
	echo "BOOTSEL volume not found: ${BOOTSEL_VOLUME}" >&2
	echo "Hold BOOTSEL while connecting the Pico W USB port, then retry." >&2
	exit 2
fi

cp "${UF2}" "${BOOTSEL_VOLUME}/"
sync
echo "Flashed ${UF2} to ${BOOTSEL_VOLUME}"
