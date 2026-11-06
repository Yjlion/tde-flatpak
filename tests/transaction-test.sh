#!/bin/bash
# GUI transaction test: install an app from a .flatpakref and uninstall it
# again, driving tdeflatpak with xdotool under Xvfb.
#
# Builds a tiny signed local repository (test runtime + test app) so no large
# downloads are needed.  Run inside the Docker build image with the repo
# mounted at /src and the built .deb in /src/dist.
#
# Usage: tests/transaction-test.sh [screenshot-dir]
set -euo pipefail

INNER=0
if [ "${1:-}" = "--inner" ]; then INNER=1; shift; fi
OUT=${1:-/tmp/screenshots}
mkdir -p "$OUT"
export HOME=/tmp/home
export PATH=/opt/trinity/bin:$PATH
APP=org.tdeflatpak.TestApp
RT=org.tdeflatpak.TestPlatform

shot() { xwd -root -silent | convert xwd:- "$OUT/$1.png"; echo "screenshot: $OUT/$1.png"; }
fail() { echo "FAIL: $*"; shot failure; exit 1; }
# there is no window manager under Xvfb: focus the newest window titled $1
# before sending keys
press() {
    local wid
    wid=$(xdotool search --onlyvisible --name "$1" | tail -1)
    [ -n "$wid" ] || fail "no window named '$1'"
    xdotool windowfocus --sync "$wid" 2>/dev/null || true
    sleep 0.5
    xdotool key "$2"
}
installed() { flatpak list --user --app --columns=application | grep -qx "$APP"; }

if [ "$INNER" = 1 ]; then
    dcopserver --nosid >/dev/null 2>&1 || true
    sleep 3

    # ---- install via .flatpakref ------------------------------------------
    tdeflatpak /tmp/test.flatpakref >"$OUT/install.log" 2>&1 &
    PID=$!
    sleep 6
    shot tx-1-confirm
    press "^Install" alt+m       # "For &Me Only" -> user installation
    for _ in $(seq 30); do installed && break; sleep 1; done
    sleep 2
    shot tx-2-installed
    installed || fail "app was not installed"
    echo "OK: installed via .flatpakref"
    press "^Install" Return      # close the operation dialog
    sleep 2
    kill $PID; wait $PID 2>/dev/null || true

    # ---- uninstall from the Installed page ---------------------------------
    tdeflatpak >"$OUT/uninstall.log" 2>&1 &
    PID=$!
    sleep 6
    WID=$(xdotool search --name "Flatpak Manager" | head -1)
    eval "$(xdotool getwindowgeometry --shell "$WID")"
    xdotool mousemove $((X + 80)) $((Y + 143)) click 1   # first row of the list
    sleep 1
    shot tx-3-selected
    press "Flatpak Manager" alt+u   # &Uninstall
    sleep 2
    shot tx-4-confirm
    press "^Uninstall" Return    # confirm (default button)
    for _ in $(seq 30); do installed || break; sleep 1; done
    sleep 2
    shot tx-5-uninstalled
    installed && fail "app is still installed"
    echo "OK: uninstalled from the Installed page"
    kill $PID; wait $PID 2>/dev/null || true
    exit 0
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq /src/dist/tdeflatpak_*_amd64.deb \
    xvfb xauth x11-xserver-utils xdotool x11-apps imagemagick gnupg >/dev/null
mkdir -p "$HOME"

# ---- build a tiny signed repository ----------------------------------------
W=/tmp/fpbuild
mkdir -p "$W" && cd "$W"
export GNUPGHOME=$W/gnupg
mkdir -m 700 -p "$GNUPGHOME"
gpg --batch --passphrase '' --quick-gen-key tdeflatpak-test@example.invalid default default never 2>/dev/null
KEYID=$(gpg --list-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
gpg --export "$KEYID" > key.gpg

mkdir -p rt/files rt/usr
printf '[Runtime]\nname=%s\nruntime=%s/x86_64/1\nsdk=%s/x86_64/1\n' "$RT" "$RT" "$RT" > rt/metadata
echo "test runtime" > rt/files/README; echo "test runtime" > rt/usr/README
flatpak build-export --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo rt 1 >/dev/null

flatpak remote-add --user --gpg-import=key.gpg testrepo "file://$W/repo"
flatpak install --user -y --noninteractive testrepo "runtime/$RT/x86_64/1" >/dev/null

flatpak build-init app "$APP" "$RT" "$RT" 1
mkdir -p app/files/bin
printf '#!/bin/sh\necho hello\n' > app/files/bin/hello
chmod +x app/files/bin/hello
flatpak build-finish --command=hello app >/dev/null
flatpak build-export --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo app master >/dev/null
flatpak build-update-repo --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo >/dev/null

cat > /tmp/test.flatpakref <<REF
[Flatpak Ref]
Title=tdeflatpak Test App
Name=$APP
Branch=master
Url=file://$W/repo
IsRuntime=false
GPGKey=$(base64 -w0 key.gpg)
REF

cd /
xvfb-run -a -s "-screen 0 1024x700x24" bash "$0" --inner "$OUT"
