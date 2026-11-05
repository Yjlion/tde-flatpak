# tdeflatpak: Flatpak Manager for the Trinity Desktop Environment

> [!WARNING]
> **Disclaimer: this project was vibe coded.**
> This application was written largely with the help of an AI coding
> assistant. It has **not been thoroughly reviewed or tested**, may contain
> bugs, and **can modify the Flatpak installations on your system** (install,
> update and uninstall software, add and remove remotes). It is provided
> **as-is, without any warranty**. Use it at your own risk and keep backups.

`tdeflatpak` is a native [Trinity Desktop Environment](https://trinitydesktop.org)
(TDE) application for managing [Flatpak](https://flatpak.org). It is written in
C++ with TQt3/tdelibs and talks to Flatpak through the **libflatpak API**,
not the `flatpak` command line tool. Its design takes ideas from GNOME
Software's flatpak plugin and Plasma Discover's Flatpak backend.

## Features

- **Installed**: lists installed applications and runtimes in both the user
  and the system installation, with filtering. You can run and uninstall them.
- **Search & Install**: searches the AppStream catalogs of all enabled remotes
  by name, ID or summary, showing icons, and installs applications.
- **Updates**: checks for updates and updates the selected items or all of them.
- **Remotes**: lists, adds (from a `.flatpakrepo` file or URL, or with a
  one-click *Add Flathub*), enables and disables, and removes remotes.
  *Remove Unused* cleans up runtimes that are no longer needed.
- Opens `.flatpakref` files (to install) and `.flatpakrepo` files (to add a
  remote) from the file manager or the command line.
- Shows live progress and a detailed log for each operation, with Cancel support.

## Installing

Download the `.deb` from the
[GitHub Actions artifacts](../../actions/workflows/build-deb.yml) or a
[release](../../releases), then:

```sh
sudo apt install ./tdeflatpak_*_amd64.deb
```

The package depends on `flatpak` and recommends:

- `polkit-agent-tde`: a polkit authentication agent. It is required for
  system-wide operations, because libflatpak asks for authorization through
  polkit.
- `xdg-desktop-portal-trinity`: the TDE backend for the XDG desktop portals,
  which Flatpak applications use for file dialogs, opening URLs and similar.

Start it from the TDE menu (*System → Flatpak Manager*) or run
`tdeflatpak [file.flatpakref|file.flatpakrepo] [--updates|--search]`.

## Building

The package is built in a Debian Trixie Docker container with TDE R14.1.x from
the official Trinity repository:

```sh
scripts/build-deb.sh          # writes dist/tdeflatpak_*.deb (runs lintian)
scripts/compile-check.sh      # quick compile-only check
```

Building manually on a Debian/TDE system with the build dependencies from
`debian/control` installed:

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/trinity
cmake --build build
```

## Tests

Both tests run inside the build image under Xvfb and drive the GUI with `xdotool`:

```sh
# install from a .flatpakref and uninstall via the GUI, using a tiny local
# signed test repository (no large downloads)
docker run --rm -v "$PWD":/src:ro -v /tmp/shots:/shots tdeflatpak-build:trixie \
    bash /src/tests/transaction-test.sh /shots

# start-up smoke test of every page; --with-flathub also loads the real
# Flathub catalog and searches it
docker run --rm -v "$PWD":/src:ro -v /tmp/shots:/shots tdeflatpak-build:trixie \
    bash /src/tests/smoke-test.sh [--with-flathub] /shots
```

The GitHub Action (`.github/workflows/build-deb.yml`) builds the package, runs
both tests and uploads the `.deb` and the screenshots as artifacts. Pushing a
`v*` tag also attaches the `.deb` to a GitHub release.

## Architecture

| File | Purpose |
| --- | --- |
| `src/flatpakbackend.*` | Synchronous libflatpak queries: installed refs, remotes, launch |
| `src/flatpakjob.*` | `TQThread` worker with its own `GMainContext`. Runs `FlatpakTransaction`s, AppStream refreshes, update checks and remote changes, and posts progress events to the GUI |
| `src/appstream.*` | Streaming parser for AppStream XML (gzip). Extracts name, summary, icon and bundle |
| `src/operationdialog.*` | Progress and log dialog for a job |
| `src/*page.*` | The four tabs |
| `src/flatpak_p.h` | The only place `<flatpak.h>` is included; it works around the GLib/TQt `signals` macro clash |

## License

GPL-2.0-or-later
