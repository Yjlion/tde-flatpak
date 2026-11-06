/*
 * tdeflatpak - human readable descriptions of sandbox permissions.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "permissiontext.h"
#include "overrides.h"

#include <tqstylesheet.h>

#include <tdelocale.h>

namespace PermissionText
{

TQString filesystem(const TQString &path)
{
    static const struct { const char *token; const char *text; } names[] = {
        { "host", I18N_NOOP("All files on the computer") },
        { "host-os", I18N_NOOP("System libraries and programs") },
        { "host-etc", I18N_NOOP("System configuration (/etc)") },
        { "home", I18N_NOOP("Home folder") },
        { "xdg-desktop", I18N_NOOP("Desktop folder") },
        { "xdg-documents", I18N_NOOP("Documents folder") },
        { "xdg-download", I18N_NOOP("Downloads folder") },
        { "xdg-music", I18N_NOOP("Music folder") },
        { "xdg-pictures", I18N_NOOP("Pictures folder") },
        { "xdg-videos", I18N_NOOP("Videos folder") },
        { "xdg-templates", I18N_NOOP("Templates folder") },
        { "xdg-public-share", I18N_NOOP("Public folder") },
        { "xdg-config", I18N_NOOP("Application settings (~/.config)") },
        { "xdg-data", I18N_NOOP("Application data (~/.local/share)") },
        { "xdg-cache", I18N_NOOP("Caches (~/.cache)") },
        { "xdg-run", I18N_NOOP("Runtime directory") },
        { 0, 0 }
    };
    for (int i = 0; names[i].token; ++i) {
        if (path == names[i].token) {
            return i18n(names[i].text);
        }
        const TQString prefix = TQString::fromLatin1(names[i].token) + "/";
        if (path.startsWith(prefix) && !path.startsWith("host")) {
            return i18n("folder description: sub path", "%1: %2").arg(i18n(names[i].text)).arg(path.mid(prefix.length()));
        }
    }
    return path;
}

TQString access(const TQString &mode)
{
    if (mode == "ro") {
        return i18n("Read-only");
    }
    if (mode == "create") {
        return i18n("Read/write, create if missing");
    }
    return i18n("Read/write");
}

LineList summarize(const FpPermissions &p)
{
    LineList risky, normal;
    Line l;

#define ADD(cond, isRisky, txt) \
    if (cond) { l.text = (txt); l.risky = (isRisky); (isRisky ? risky : normal).append(l); }

    for (TQStringList::ConstIterator it = p.filesystems.begin(); it != p.filesystems.end(); ++it) {
        const TQString path = Overrides::filesystemPath(*it);
        const TQString mode = Overrides::filesystemMode(*it);
        const bool bad = (path == "host" || path == "host-os" || path == "host-etc" || path == "home"
                          || path.startsWith("~") || path == "/") && mode != "ro";
        ADD(true, bad, i18n("Files: %1 (%2)").arg(filesystem(path)).arg(access(mode).lower()));
    }
    ADD(p.shared.contains("network"), false, i18n("Network access"));
    ADD(p.sockets.contains("x11") , true,
        i18n("X11 display: can read keystrokes and the contents of other windows"));
    ADD(p.sockets.contains("fallback-x11") && !p.sockets.contains("x11"), true,
        i18n("X11 display when Wayland is not available (always the case in TDE)"));
    ADD(p.sockets.contains("wayland"), false, i18n("Wayland display"));
    ADD(p.sockets.contains("pulseaudio"), false, i18n("Sound playback and recording"));
    ADD(p.devices.contains("all"), true, i18n("All devices, including cameras"));
    ADD(p.devices.contains("dri") && !p.devices.contains("all"), false, i18n("Graphics acceleration"));
    ADD(p.devices.contains("input"), false, i18n("Input devices (game controllers)"));
    ADD(p.devices.contains("usb"), false, i18n("USB devices"));
    ADD(p.devices.contains("kvm"), false, i18n("Virtualization (KVM)"));
    ADD(p.sockets.contains("session-bus"), true, i18n("Full access to the session bus"));
    ADD(p.sockets.contains("system-bus"), true, i18n("Full access to the system bus"));
    ADD(p.sockets.contains("ssh-auth"), true, i18n("Your SSH keys (SSH agent)"));
    ADD(p.sockets.contains("gpg-agent"), true, i18n("Your GnuPG keys (gpg-agent)"));
    ADD(p.sockets.contains("pcsc"), false, i18n("Smart cards"));
    ADD(p.sockets.contains("cups"), false, i18n("Printing system"));
    ADD(p.features.contains("bluetooth"), false, i18n("Bluetooth"));
    ADD(p.features.contains("devel"), true, i18n("Debugging and profiling system calls"));

    TQMap<TQString, TQString>::ConstIterator it;
    for (it = p.sessionBus.begin(); it != p.sessionBus.end(); ++it) {
        const bool escape = it.key() == "org.freedesktop.Flatpak" || it.key().startsWith("org.freedesktop.impl.portal")
                            || it.key() == "org.freedesktop.systemd1";
        if (escape) {
            ADD(true, true, i18n("Can run commands outside the sandbox (%1)").arg(it.key()));
        } else if (it.data() == "own") {
            ADD(true, false, i18n("Provides the service %1").arg(it.key()));
        } else if (it.data() == "talk") {
            ADD(true, false, i18n("Talks to %1").arg(it.key()));
        }
    }
    for (it = p.systemBus.begin(); it != p.systemBus.end(); ++it) {
        if (it.data() == "talk" || it.data() == "own") {
            ADD(true, false, i18n("Talks to the system service %1").arg(it.key()));
        }
    }
#undef ADD

    LineList out = risky;
    for (LineList::ConstIterator n = normal.begin(); n != normal.end(); ++n) {
        out.append(*n);
    }
    return out;
}

TQString html(const FpPermissions &p)
{
    const LineList lines = summarize(p);
    if (lines.isEmpty()) {
        return i18n("<p>No special permissions: the application is fully sandboxed.</p>");
    }
    TQString out = "<ul>";
    for (LineList::ConstIterator it = lines.begin(); it != lines.end(); ++it) {
        const TQString text = TQStyleSheet::escape((*it).text);
        if ((*it).risky) {
            out += "<li><font color=\"#b04000\"><b>" + text + "</b></font></li>";
        } else {
            out += "<li>" + text + "</li>";
        }
    }
    return out + "</ul>";
}

}
