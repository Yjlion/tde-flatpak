#!/bin/sh
# Build the tdeflatpak Debian package inside a Debian Docker container
# (TDE R14.1.x).  The resulting .deb files are written to ./dist.
#
# SUITE selects the Debian release: trixie (default) or bookworm.  A
# bookworm build gets "~bookworm" appended to its version, so its file
# names differ from the trixie build and it sorts below it.
#
# Usage: [SUITE=bookworm] scripts/build-deb.sh [--no-lintian]
set -eu

cd "$(dirname "$0")/.."
ROOT=$(pwd)
SUITE=${SUITE:-trixie}
IMAGE=${IMAGE:-tdeflatpak-build:$SUITE}
LINTIAN=1
[ "${1:-}" = "--no-lintian" ] && LINTIAN=0

docker build --build-arg SUITE="$SUITE" -t "$IMAGE" docker/

mkdir -p "$ROOT/dist"

docker run --rm \
    -e LINTIAN="$LINTIAN" -e SUITE="$SUITE" \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    -v "$ROOT":/src:ro \
    -v "$ROOT/dist":/dist \
    "$IMAGE" bash -euc '
        mkdir -p /build
        cp -a /src /build/tdeflatpak
        rm -rf /build/tdeflatpak/dist /build/tdeflatpak/.git
        cd /build/tdeflatpak
        if [ "$SUITE" != trixie ]; then
            VERSION=$(dpkg-parsechangelog -S Version)
            DEBFULLNAME="GitHub Actions" DEBEMAIL=actions@users.noreply.github.com \
                dch -b --newversion "${VERSION}~${SUITE}" --distribution "$SUITE" \
                    --force-distribution "Build for Debian ${SUITE}."
        fi
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
