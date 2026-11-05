/*
 * tdeflatpak - private libflatpak include helper.
 *
 * GLib/GIO headers use "signals" as an identifier, which collides with the
 * TQt "signals" keyword macro.  Include this header *last* in .cpp files that
 * need libflatpak, and never from a header that is processed by moc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TDEFLATPAK_FLATPAK_P_H
#define TDEFLATPAK_FLATPAK_P_H

#include "flatpaktypes.h"

#include <tqcstring.h>

#undef signals
#undef slots
#undef emit
#include <flatpak.h>

/** UTF-8 C string (may be NULL) -> TQString */
inline TQString fpStr(const char *s)
{
    return s ? TQString::fromUtf8(s) : TQString::null;
}

/** Open the user or system installation. */
FlatpakInstallation *fpOpenInstallation(int scope, GCancellable *cancellable, GError **error);

/** Convert a libflatpak installed ref into our plain data type. */
FpInstalledRef fpConvertInstalledRef(FlatpakInstalledRef *ref, int scope);

/** Take ownership of a GError: returns its message and frees it. */
TQString fpTakeError(GError *&error);

#endif
