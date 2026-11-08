#!/bin/bash
# Smoke test for the built package.  Run inside a Debian Trixie container with
# the Trinity repository configured (see docker/Dockerfile) and the repository
# mounted at /src (the .deb files in /src/dist).
#
#  - installs the .deb (pulls in flatpak and TDE runtime libraries)
#  - optionally adds Flathub to the *user* installation (needs network)
#  - starts tdeflatpak under Xvfb on every page and takes screenshots
#
# Usage: tests/smoke-test.sh [--with-flathub] [screenshot-dir]
set -euo pipefail

INNER=0
if [ "${1:-}" = "--inner" ]; then INNER=1; shift; fi
WITH_FLATHUB=${WITH_FLATHUB:-0}
if [ "${1:-}" = "--with-flathub" ]; then WITH_FLATHUB=1; shift; fi
OUT=${1:-/tmp/screenshots}
mkdir -p "$OUT"
export HOME=/tmp/home
export PATH=/opt/trinity/bin:$PATH

shot() {  # $1 = name
    sleep "${2:-4}"
    xwd -root -silent | convert xwd:- "$OUT/$1.png"
    echo "screenshot: $OUT/$1.png"
}

run_page() {  # $1 = name, rest = args
    local name=$1; shift
    tdeflatpak "$@" >"$OUT/$name.log" 2>&1 &
    local pid=$!
    shot "$name" "${WAIT:-6}"
    if ! kill -0 $pid 2>/dev/null; then
        echo "tdeflatpak exited early on page $name:"; cat "$OUT/$name.log"; return 1
    fi
    if [ "$name" = search ]; then
        # type a query into the search field (focused line edit)
        WID=$(xdotool search --name "Flatpak Manager" | head -1)
        xdotool windowactivate --sync "$WID" 2>/dev/null || true
        # the Flathub catalog takes a while to download and parse
        if [ "${WITH_FLATHUB:-0}" = 1 ]; then sleep "${CATALOG_WAIT:-60}"; else sleep 3; fi
        xdotool key --window "$WID" alt+e
        xdotool type --delay 50 "gimp"
        shot search-gimp 3
    fi
    kill $pid; wait $pid 2>/dev/null || true
}

if [ "$INNER" = 1 ]; then
    # a TDE session normally provides the DCOP server and tdeinit
    dcopserver --nosid >/dev/null 2>&1 || true
    sleep 3
    run_page installed
    run_page updates --updates
    run_page search --search
    if [ "${WITH_FLATHUB:-0}" = 1 ]; then
        # the catalog and the Flathub web service need a while
        WAIT=${CATALOG_WAIT:-60} run_page browse --browse
        WAIT=${CATALOG_WAIT:-60} run_page details --details org.gimp.GIMP
    else
        run_page browse --browse
    fi
    tdeflatpak --version
    exit 0
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq /src/dist/tde-flatpak-trinity_*_amd64.deb xvfb xauth x11-xserver-utils xdotool x11-apps imagemagick >/dev/null
mkdir -p "$HOME"

if [ "$WITH_FLATHUB" = 1 ]; then
    flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
fi

export WITH_FLATHUB
xvfb-run -a -s "-screen 0 1024x700x24" bash "$0" --inner "$OUT"
