#!/usr/bin/env bash
# Builds the firmware and produces the release image:
#
#   dist/geoscout-app.bin      raw app image  -> M5Launcher (SD/WebUI/OTA)
#
# Only the app image ships. M5Launcher is how this firmware gets installed, and
# anyone with the toolchain checked out flashes over USB with
# `pio run -e cardputer-adv -t upload`.
#
# Usage: tools/package.sh [output-dir]
set -euo pipefail

NAME="geoscout"
ENVIRONMENT="cardputer-adv"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/dist}"
BUILD="$ROOT/.pio/build/$ENVIRONMENT"

# M5Launcher writes app images into an OTA partition, so the app must stay
# comfortably inside one. Fail the build rather than ship something the
# launcher would reject. 0x330000 is the app slot in default_8MB.csv.
MAX_APP_BYTES=$((3342336))

PIO="${PIO:-pio}"

cd "$ROOT"
"$PIO" run -e "$ENVIRONMENT"

APP="$BUILD/firmware.bin"

app_size=$(stat -c%s "$APP")
echo "app image: $app_size bytes (limit $MAX_APP_BYTES)"
if [ "$app_size" -gt "$MAX_APP_BYTES" ]; then
    echo "ERROR: app image will not fit an OTA partition" >&2
    exit 1
fi

# The app image must start with the ESP image magic or Launcher will not
# recognise it as an application binary.
magic=$(od -An -tx1 -N1 "$APP" | tr -d ' \n')
if [ "$magic" != "e9" ]; then
    echo "ERROR: app image magic is 0x$magic, expected 0xe9" >&2
    exit 1
fi

mkdir -p "$OUT"
cp "$APP" "$OUT/$NAME-app.bin"

echo
echo "artifacts in $OUT:"
ls -l "$OUT"
