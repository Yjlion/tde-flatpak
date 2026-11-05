#!/bin/sh
# Quick out-of-tree compile inside the Docker build image (no packaging).
set -e
cd "$(dirname "$0")/.."
docker build -q -t "${IMAGE:-tdeflatpak-build:trixie}" docker/ >/dev/null
docker run --rm -v "$PWD":/src:ro "${IMAGE:-tdeflatpak-build:trixie}" bash -c '
  mkdir -p /b && cd /b
  cmake /src -DCMAKE_INSTALL_PREFIX=/opt/trinity >/tmp/cmake.log 2>&1 || { tail -40 /tmp/cmake.log; exit 1; }
  make -j"$(nproc)" -k 2>&1 | grep -E "error|warning" -A3 | head -${LINES:-120}
  make 2>&1 | tail -2
'
