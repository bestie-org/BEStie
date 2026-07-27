#!/bin/bash

# Build firmware for all boards defined in SUPPORTED_BOARDS in nrf5_sdk.mk

set -euo pipefail

BUILD="${BUILD:-release}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

DEPS=(grep make python3)
MISSING=()

for cmd in "${DEPS[@]}"; do
	if ! command -v "$cmd" &>/dev/null; then
		MISSING+=("$cmd")
	fi
done

if ((${#MISSING[@]})); then
	echo "Missing dependencies: ${MISSING[*]}" >&2
	exit 1
fi

read -ra BOARDS < <(grep 'SUPPORTED_BOARDS := ' nrf5_sdk.mk | cut -d '=' -f2)

rm -rf ./build/

for BOARD in "${BOARDS[@]}"; do
	echo "Building in $BUILD mode for $BOARD"
	make -j BUILD="${BUILD}" BOARD="${BOARD}"
done
