#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

mkdir -p dist

echo "==> Building project from $ROOT"

# Build Linux host dump (works on Linux/macOS with gcc)
if command -v gcc >/dev/null 2>&1; then
  echo "==> Building host dump"
  make host
  cp build/host/ps5greenapple-dump dist/ps5greenapple-dump-host || true
fi

# Build Linux screenshots if host dump exists
if [[ -x build/host/ps5greenapple-dump ]]; then
  echo "==> Rendering screenshots"
  make shots || true
  tar -czf dist/screenshots.tar.gz shots || true
fi

# Build macOS sim target if on macOS
if [[ "$(uname)" == "Darwin" ]]; then
  echo "==> Building macOS SDL sim"
  if ! command -v sdl2-config >/dev/null 2>&1; then
    echo "SDL2 not found. Install with: brew install sdl2"
    echo "Skipping macOS sim build."
  else
    make sim
    cp build/sim/ps5greenapple-sim dist/ps5greenapple-sim-macos || true
  fi
fi

# Build PS5 ELF if SDK is installed or exported
SDK_PATH="${PS5_PAYLOAD_SDK:-/opt/ps5-sdk}"
if [[ -d "$SDK_PATH" && -f "$SDK_PATH/toolchain/prospero.mk" ]]; then
  echo "==> Building PS5 ELF using $SDK_PATH"
  PS5_PAYLOAD_SDK="$SDK_PATH" make
  cp ps5greenapple.elf dist/ps5greenapple-elf || true
else
  echo "==> PS5 SDK not found at $SDK_PATH"
  echo "Set PS5_PAYLOAD_SDK or install it under /opt/ps5-sdk to build the PS5 ELF."
fi

echo
echo "Artifacts:"
find dist -maxdepth 2 -type f -print | sort
