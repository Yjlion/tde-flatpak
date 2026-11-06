#!/bin/bash
# GUI test of the 0.2 features: app details, the Browse page, the permission
# editor (overrides, portal permissions, blocking updates, deleting app data)
# and the Control Center module.  Drives tdeflatpak with xdotool under Xvfb
# and checks the effects with the flatpak command line tool.
#
# Builds a tiny signed local repository with AppStream data, so no network
# access is needed.  Run inside the Docker build image with the repository
# mounted at /src and the built .deb in /src/dist.
#
# Usage: tests/features-test.sh [screenshot-dir]
set -euo pipefail

INNER=0
if [ "${1:-}" = "--inner" ]; then INNER=1; shift; fi
OUT=${1:-/tmp/screenshots}
mkdir -p "$OUT"
export HOME=/tmp/home
export PATH=/opt/trinity/bin:$PATH
APP=org.tdeflatpak.TestApp
RT=org.tdeflatpak.TestPlatform
OVERRIDE=$HOME/.local/share/flatpak/overrides/$APP

shot() { xwd -root -silent | convert xwd:- "$OUT/$1.png"; echo "screenshot: $OUT/$1.png"; }
fail() { echo "FAIL: $*"; shot "failure-$(date +%s)"; exit 1; }
wait_window() {  # $1 = title regexp; waits up to 30 s
    for _ in $(seq 60); do
        xdotool search --onlyvisible --name "$1" >/dev/null 2>&1 && return 0
        sleep 0.5
    done
    fail "no window named '$1'"
}
# there is no window manager under Xvfb: focus the newest window titled $1
# before sending keys
press() {
    local wid
    wait_window "$1"
    wid=$(xdotool search --onlyvisible --name "$1" | tail -1)
    xdotool windowfocus --sync "$wid" 2>/dev/null || true
    sleep 0.5
    xdotool key "$2"
    sleep 0.5
}
stop() { kill "$1" 2>/dev/null || true; wait "$1" 2>/dev/null || true; }
masked() { flatpak mask --user 2>/dev/null | grep -q "$APP"; }

if [ "$INNER" = 1 ]; then
    dcopserver --nosid >/dev/null 2>&1 || true
    sleep 2
    tdeinit >/dev/null 2>&1 || true
    tdebuildsycoca --noincremental >/dev/null 2>&1 || true
    sleep 2

    # ---- details dialog (local AppStream data) -----------------------------
    tdeflatpak --details "$APP" >"$OUT/details.log" 2>&1 &
    PID=$!
    wait_window "Application Details"
    sleep 4
    shot feat-1-details
    press "Application Details" Escape
    stop $PID
    echo "OK: details dialog"

    # ---- Browse page --------------------------------------------------------
    tdeflatpak --browse >"$OUT/browse.log" 2>&1 &
    PID=$!
    wait_window "Flatpak Manager"
    sleep 6
    shot feat-2-browse
    stop $PID
    echo "OK: browse page"

    # ---- permission override: revoke network access --------------------------
    tdeflatpak --permissions "$APP" >"$OUT/perm.log" 2>&1 &
    PID=$!
    wait_window "Application Permissions"
    sleep 2
    press "Application Permissions" alt+p   # Permissions tab
    press "Application Permissions" alt+n   # uncheck "Network access"
    shot feat-3-permissions
    press "Application Permissions" Return  # OK
    sleep 2
    stop $PID
    [ -f "$OVERRIDE" ] || fail "no override file written"
    grep -qx "shared=!network;" "$OVERRIDE" || fail "unexpected override file: $(cat "$OVERRIDE")"
    flatpak override --user --show "$APP" | grep -q "shared=!network;" || fail "flatpak does not see the override"
    echo "OK: network access revoked ($(tr '\n' ' ' < "$OVERRIDE"))"

    # ---- reset the override ------------------------------------------------
    tdeflatpak --permissions "$APP" >"$OUT/perm-reset.log" 2>&1 &
    PID=$!
    press "Application Permissions" alt+r   # Reset to Defaults
    press "Application Permissions" Return  # OK
    sleep 2
    stop $PID
    [ -f "$OVERRIDE" ] && fail "override file still exists after reset: $(cat "$OVERRIDE")"
    echo "OK: overrides reset"

    # ---- portal permissions ------------------------------------------------
    flatpak permission-set notifications notification "$APP" yes
    flatpak permission-show "$APP" | grep -q notification || fail "could not seed the permission store"
    tdeflatpak --permissions "$APP" >"$OUT/perm-portal.log" 2>&1 &
    PID=$!
    press "Application Permissions" alt+t   # Portals tab
    sleep 2
    shot feat-4-portals
    press "Application Permissions" alt+l   # Revoke All
    press "Revoke Permissions" Return       # confirm
    sleep 1
    press "Application Permissions" Escape
    stop $PID
    flatpak permission-show "$APP" | grep -q notification && fail "portal permission was not revoked"
    echo "OK: portal permission revoked"

    # ---- block and allow updates (flatpak mask) ----------------------------
    tdeflatpak --permissions "$APP" >"$OUT/perm-mask.log" 2>&1 &
    PID=$!
    press "Application Permissions" alt+s   # Storage & Updates tab
    press "Application Permissions" alt+u   # Block updates of the user installation
    sleep 4
    shot feat-5-storage
    masked || fail "application was not masked"
    echo "OK: updates blocked"
    press "Application Permissions" alt+u   # allow again
    sleep 4
    masked && fail "application is still masked"
    echo "OK: updates allowed again"

    # ---- delete application data --------------------------------------------
    mkdir -p "$HOME/.var/app/$APP/config"
    echo "x" > "$HOME/.var/app/$APP/config/settings"
    press "Application Permissions" alt+p   # switch tabs so the storage tab refreshes
    press "Application Permissions" alt+s
    sleep 1
    press "Application Permissions" alt+d   # Delete Data...
    press "Delete Application Data" Return  # confirm
    sleep 3
    [ -d "$HOME/.var/app/$APP" ] && fail "application data was not deleted"
    echo "OK: application data deleted"
    press "Application Permissions" Escape
    stop $PID

    # ---- Control Center module ---------------------------------------------
    # the module must be where TDE looks for modules, else tdecmshell only
    # shows an error page (which a window title check cannot tell apart)
    found=""
    for d in $(tde-config --path module | tr ':' ' '); do
        [ -f "$d/kcm_tdeflatpak.so" ] && found="$d"
    done
    [ -n "$found" ] || fail "kcm_tdeflatpak.so is not in the module path ($(tde-config --path module))"
    echo "OK: module installed in $found"
    tdecmshell tdeflatpak-settings >"$OUT/kcm.log" 2>&1 &
    PID=$!
    wait_window "Control Module"
    sleep 4
    shot feat-6-kcm
    kill -0 $PID 2>/dev/null || fail "tdecmshell exited: $(cat "$OUT/kcm.log")"
    grep -qi "could not find\|error when loading" "$OUT/kcm.log" && fail "module did not load: $(cat "$OUT/kcm.log")"
    stop $PID
    echo "OK: Control Center module"
    exit 0
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq /src/dist/tdeflatpak_*_amd64.deb \
    xvfb xauth x11-xserver-utils xdotool x11-apps imagemagick gnupg \
    dbus dbus-x11 xdg-desktop-portal >/dev/null
mkdir -p "$HOME"

# tdebuildsycoca only indexes applications (and thus Control Center modules)
# through tde-applications.menu, which tdebase ships; the build image only
# has tdelibs, so provide a minimal menu.
mkdir -p "$HOME/.config/menus"
cat > "$HOME/.config/menus/tde-applications.menu" <<'MENU'
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd">
<Menu><Name>Applications</Name><DefaultAppDirs/><DefaultDirectoryDirs/><Include><All/></Include></Menu>
MENU

# ---- build a tiny signed repository with AppStream data ---------------------
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
flatpak build-update-repo --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo >/dev/null
flatpak remote-add --user --gpg-import=key.gpg testrepo "file://$W/repo"
flatpak install --user -y --noninteractive testrepo "runtime/$RT/x86_64/1" >/dev/null

flatpak build-init app "$APP" "$RT" "$RT" 1
mkdir -p app/files/bin app/files/share/app-info/xmls app/files/share/app-info/icons/flatpak/64x64
printf '#!/bin/sh\necho hello\n' > app/files/bin/hello
chmod +x app/files/bin/hello
convert -size 64x64 xc:steelblue -fill white -draw "circle 32,32 32,8" \
    "app/files/share/app-info/icons/flatpak/64x64/$APP.png"
cat <<XML | gzip > "app/files/share/app-info/xmls/$APP.xml.gz"
<?xml version="1.0" encoding="UTF-8"?>
<components version="0.8" origin="flatpak">
  <component type="desktop-application">
    <id>$APP</id>
    <name>tdeflatpak Test App</name>
    <summary>A tiny application used by the tdeflatpak tests</summary>
    <developer_name>The tdeflatpak authors</developer_name>
    <project_license>GPL-2.0-or-later</project_license>
    <description>
      <p>This application only prints <em>hello</em>. It exists to test the details dialog.</p>
      <ul><li>Network access</li><li>Access to the home folder</li></ul>
    </description>
    <categories><category>AudioVideo</category><category>Player</category></categories>
    <url type="homepage">https://github.com/Yjlion/tde-flatpak</url>
    <icon type="cached" height="64" width="64">$APP.png</icon>
    <releases><release version="1.0" timestamp="1762300800"><description><p>First release.</p></description></release></releases>
  </component>
</components>
XML
flatpak build-finish --command=hello --share=network --socket=x11 --filesystem=home \
    --talk-name=org.freedesktop.Notifications app >/dev/null
flatpak build-export --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo app master >/dev/null
flatpak build-update-repo --gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME" repo >/dev/null

flatpak update --user --appstream testrepo >/dev/null
flatpak install --user -y --noninteractive testrepo "app/$APP/x86_64/master" >/dev/null
flatpak list --user --app --columns=application | grep -qx "$APP" || { echo "FAIL: test app not installed"; exit 1; }

cd /
dbus-run-session -- xvfb-run -a -s "-screen 0 1024x700x24" bash "$0" --inner "$OUT"
