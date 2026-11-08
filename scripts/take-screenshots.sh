#!/bin/bash
# Regenerate the screenshots in screenshots/ (used by the README and the
# handbook).  Runs tdeflatpak inside the Debian Trixie build image under Xvfb
# with a small demo repository (three example apps with AppStream data) and the
# real Flathub remote, and captures each window with ImageMagick.
#
# Usage: scripts/take-screenshots.sh        (needs dist/tde-flatpak-trinity_*.deb,
#                                            see scripts/build-deb.sh)
set -euo pipefail

if [ "${1:-}" != "--inner" ] && [ "${1:-}" != "--container" ]; then
    cd "$(dirname "$0")/.."
    IMAGE=${IMAGE:-tdeflatpak-build:trixie}
    ls dist/tde-flatpak-trinity_*_amd64.deb >/dev/null
    mkdir -p screenshots
    docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
        -v "$PWD":/src:ro -v "$PWD/screenshots":/shots "$IMAGE" \
        bash /src/scripts/take-screenshots.sh --container
    exit 0
fi

OUT=/shots
export HOME=/tmp/home
export PATH=/opt/trinity/bin:$PATH
W=/tmp/demo
ARCH=$(flatpak --default-arch 2>/dev/null || echo x86_64)

window() {  # newest visible window whose title matches $1
    local wid=""
    for _ in $(seq 20); do
        wid=$(xdotool search --onlyvisible --name "$1" 2>/dev/null | tail -1 || true)
        [ -n "$wid" ] && break
        sleep 0.5
    done
    [ -n "$wid" ] || { echo "no window '$1'" >&2; return 1; }
    echo "$wid"
}
grab() {  # $1 = window title regex, $2 = file name
    local wid; wid=$(window "$1")
    import -window "$wid" "$OUT/$2.png"
    echo "screenshot: $2.png"
}
press() {  # $1 = window title regex, $2 = key
    local wid; wid=$(window "$1")
    xdotool windowfocus --sync "$wid" 2>/dev/null || true
    sleep 0.3
    xdotool key "$2"
}
start() {  # start tdeflatpak with arguments, wait for the main window
    tdeflatpak -geometry 940x600+0+0 "$@" >>"$W/tdeflatpak.log" 2>&1 &
    PID=$!
    window "Flatpak Manager" >/dev/null
    sleep 3
}
stop() { kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null || true; }

if [ "$1" = "--inner" ]; then
    dcopserver --nosid >/dev/null 2>&1 || true
    sleep 3

    start
    grab "Flatpak Manager" installed
    stop

    start --search
    sleep "${CATALOG_WAIT:-45}"           # Flathub catalog download + parse
    press "Flatpak Manager" alt+e        # S&earch
    xdotool type --delay 40 "image editor"
    sleep 2
    grab "Flatpak Manager" search
    stop

    start --updates
    sleep 5
    grab "Flatpak Manager" updates
    press "Flatpak Manager" alt+a        # Update &All
    sleep 4
    grab "^Update" operation
    press "^Update" Return
    sleep 1
    stop

    start --browse
    sleep "${CATALOG_WAIT:-45}"
    grab "Flatpak Manager" browse
    stop

    start --details org.gimp.GIMP
    window "Application Details" >/dev/null
    sleep 15                             # Flathub web API and screenshot download
    grab "Application Details" details
    press "Application Details" Escape
    stop

    start --permissions org.example.Notes
    window "Application Permissions" >/dev/null
    sleep 2
    grab "Application Permissions" permissions
    press "Application Permissions" Escape
    stop

    tdebuildsycoca --noincremental >/dev/null 2>&1 || true
    tdecmshell tdeflatpak-settings >>"$W/tdeflatpak.log" 2>&1 &
    PID=$!
    window "Control Module" >/dev/null
    sleep 3
    grab "Control Module" kcm
    stop

    start --remotes
    grab "Flatpak Manager" remotes
    press "Flatpak Manager" alt+a        # &Add...
    sleep 2
    press "^Add Remote" ctrl+a
    xdotool type --delay 20 "https://dl.flathub.org/beta-repo/flathub-beta.flatpakrepo"
    sleep 1
    grab "^Add Remote" add-remote
    press "^Add Remote" Escape
    stop
    exit 0
fi

# ---------------------------------------------------------------- container
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq /src/dist/tde-flatpak-trinity_*_amd64.deb \
    xvfb xauth x11-xserver-utils xdotool imagemagick gnupg fonts-dejavu-core >/dev/null
mkdir -p "$HOME" "$W"
cd "$W"

# tdebuildsycoca indexes Control Center modules through tde-applications.menu,
# which comes with tdebase; the build image only has tdelibs
mkdir -p "$HOME/.config/menus"
cat > "$HOME/.config/menus/tde-applications.menu" <<'MENU'
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd">
<Menu><Name>Applications</Name><DefaultAppDirs/><DefaultDirectoryDirs/><Include><All/></Include></Menu>
MENU

# bubblewrap cannot create namespaces in an unprivileged container; flatpak
# only needs it here to sandbox its icon validator, so run that directly
cat > "$W/fake-bwrap" <<'SH'
#!/bin/bash
# exec the command after the last flatpak-validate-icon path (earlier
# occurrences are bind-mount arguments)
cmd=()
for arg in "$@"; do
    case "$arg" in */flatpak-validate-icon) cmd=("$arg"); continue ;; esac
    [ ${#cmd[@]} -gt 0 ] && cmd+=("$arg")
done
if [ ${#cmd[@]} -eq 0 ]; then   # e.g. flatpak triggers: run what follows "--"
    while [ $# -gt 0 ] && [ "$1" != "--" ]; do shift; done
    shift || true
    cmd=("$@")
fi
[ ${#cmd[@]} -gt 0 ] || exit 1
exec "${cmd[@]}"
SH
chmod +x "$W/fake-bwrap"
export FLATPAK_BWRAP=$W/fake-bwrap

export GNUPGHOME=$W/gnupg
mkdir -m 700 -p "$GNUPGHOME"
gpg --batch --passphrase '' --quick-gen-key demo@example.invalid default default never 2>/dev/null
KEYID=$(gpg --list-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
gpg --export "$KEYID" > key.gpg
SIGN=(--gpg-sign="$KEYID" --gpg-homedir="$GNUPGHOME")

RT=org.example.Platform
mkdir -p rt/files rt/usr
printf '[Runtime]\nname=%s\nruntime=%s/%s/24.08\nsdk=%s/%s/24.08\n' "$RT" "$RT" "$ARCH" "$RT" "$ARCH" > rt/metadata
echo "demo runtime" > rt/files/README
flatpak build-export "${SIGN[@]}" repo rt 24.08 >/dev/null
flatpak build-update-repo "${SIGN[@]}" --title="Example Apps" repo >/dev/null
flatpak remote-add --user --gpg-import=key.gpg --title="Example Apps" demo "file://$W/repo"
flatpak install --user -y --noninteractive demo "runtime/$RT/$ARCH/24.08" >/dev/null

# helper: build_app <id> <name> <summary> <version> <color> <letter>
build_app() {
    local id=$1 name=$2 summary=$3 version=$4 color=$5 letter=$6
    rm -rf "app-$id"
    flatpak build-init "app-$id" "$id" "$RT" "$RT" 24.08 >/dev/null
    local f="app-$id/files"
    mkdir -p "$f/bin" "$f/share/app-info/xmls" "$f/share/app-info/icons/flatpak/64x64" "$f/share/applications"
    printf '#!/bin/sh\necho %s\n' "$name" > "$f/bin/run"
    chmod +x "$f/bin/run"
    convert -size 64x64 xc:none -fill "$color" -draw "roundrectangle 3,3 60,60 14,14" \
        -fill white -font DejaVu-Sans-Bold -pointsize 34 -gravity center -annotate 0 "$letter" \
        "$f/share/app-info/icons/flatpak/64x64/$id.png"
    cat > comp.xml <<XML
<?xml version="1.0" encoding="UTF-8"?>
<components version="0.8" origin="flatpak">
  <component type="desktop-application">
    <id>$id</id>
    <name>$name</name>
    <summary>$summary</summary>
    <icon type="cached" width="64" height="64">$id.png</icon>
    <categories><category>Utility</category></categories>
    <releases><release version="$version" date="2026-11-01"/></releases>
    <bundle type="flatpak">app/$id/$ARCH/stable</bundle>
  </component>
</components>
XML
    gzip -c comp.xml > "$f/share/app-info/xmls/$id.xml.gz"
    flatpak build-finish --command=run --share=network --socket=x11 --filesystem=xdg-documents "app-$id" >/dev/null
    # exported icon added after build-finish: its icon validation needs bwrap,
    # which does not work inside an unprivileged container
    mkdir -p "app-$id/export/share/icons/hicolor/64x64/apps"
    cp "$f/share/app-info/icons/flatpak/64x64/$id.png" "app-$id/export/share/icons/hicolor/64x64/apps/"
    flatpak build-export "${SIGN[@]}" repo "app-$id" stable >/dev/null
}

build_app org.example.Notes      "Notes"        "Write down quick notes and ideas" 1.2 "#e0a100" N
build_app org.example.Calculator "Calculator"   "Perform simple and scientific calculations" 3.0 "#2f7d32" C
build_app org.example.PhotoView  "Photo Viewer" "Browse and view your image collection" 0.9 "#1f5fa8" P
flatpak build-update-repo "${SIGN[@]}" --title="Example Apps" repo >/dev/null

flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive demo org.example.Notes org.example.Calculator >/dev/null

# publish new versions so that the Updates page has something to show
build_app org.example.Notes      "Notes"      "Write down quick notes and ideas" 1.3 "#e0a100" N
build_app org.example.Calculator "Calculator" "Perform simple and scientific calculations" 3.1 "#2f7d32" C
flatpak build-update-repo "${SIGN[@]}" --title="Example Apps" repo >/dev/null

xvfb-run -a -s "-screen 0 1280x800x24" bash "$0" --inner
chown -R "$HOST_UID:$HOST_GID" "$OUT"
