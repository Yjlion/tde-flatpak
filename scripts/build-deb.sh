#!/bin/sh
# Build the tdeflatpak Debian package inside a Debian Trixie Docker
# container (TDE R14.1.x).  The resulting .deb files are written to ./dist.
#
# Usage: scripts/build-deb.sh [--no-lintian]
set -eu

cd "$(dirname "$0")/.."
ROOT=$(pwd)
IMAGE=${IMAGE:-tdeflatpak-build:trixie}
LINTIAN=1
[ "${1:-}" = "--no-lintian" ] && LINTIAN=0

docker build -t "$IMAGE" docker/

mkdir -p "$ROOT/dist"

docker run --rm \
    -e LINTIAN="$LINTIAN" \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    -v "$ROOT":/src:ro \
    -v "$ROOT/dist":/dist \
    "$IMAGE" bash -euc '
        mkdir -p /build
        cp -a /src /build/tdeflatpak
        rm -rf /build/tdeflatpak/dist /build/tdeflatpak/.git
        cd /build/tdeflatpak
        dpkg-buildpackage -us -uc -b
        cd /build
        if [ "$LINTIAN" = 1 ]; then
            lintian --info --display-info *.changes || echo "lintian reported issues (non-fatal)"
        fi
        cp -v /build/*.deb /build/*.buildinfo /build/*.changes /dist/
        chown -R "$HOST_UID:$HOST_GID" /dist
    '

echo
echo "Packages written to $ROOT/dist:"
ls -l "$ROOT/dist"
