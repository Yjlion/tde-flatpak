/*
 * tdeflatpak - unit tests for the parts that need neither a display nor
 * a Flatpak installation: Flathub JSON parsing, AppStream parsing, user
 * permission overrides and mask/pin pattern matching.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appstream.h"
#include "flatpakbackend.h"
#include "flathubjson.h"
#include "overrides.h"

#include <tqdir.h>
#include <tqfile.h>

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { ++checks; if (!((a) == (b))) { ++failures; \
    fprintf(stderr, "FAIL %s:%d: %s == %s (got \"%s\")\n", __FILE__, __LINE__, #a, #b, \
            TQString(a).local8Bit().data()); } } while (0)

static TQByteArray fixture(const char *name)
{
    TQFile f(TQString(FIXTURE_DIR) + "/" + name);
    if (!f.open(IO_ReadOnly)) {
        fprintf(stderr, "cannot open fixture %s\n", name);
        exit(2);
    }
    return f.readAll();
}

static void testFlathubJson()
{
    TQString error;
    FpAppDetails d;
    CHECK(FlathubJson::parseAppstream(fixture("appstream_org.gimp.GIMP.json"), d, error));
    CHECK_EQ(d.id, TQString("org.gimp.GIMP"));
    CHECK_EQ(d.name, TQString("GNU Image Manipulation Program"));
    CHECK(d.description.contains("<p>"));
    CHECK(!d.screenshots.isEmpty());
    CHECK(!d.screenshots.isEmpty() && d.screenshots.first().imageUrl.startsWith("https://"));
    CHECK(!d.screenshots.isEmpty() && d.screenshots.first().thumbnailUrl.contains("624x351"));
    CHECK(!d.releases.isEmpty() && d.releases.first().timestamp > 0);
    CHECK_EQ(d.urls["homepage"], TQString("https://www.gimp.org/"));
    CHECK(!d.urls.contains("translate"));   // null in the document
    CHECK(d.categories.contains("Graphics"));
    CHECK(d.runtime.startsWith("org.gnome.Platform/"));

    FpAppDetails s;
    CHECK(FlathubJson::parseSummary(fixture("summary_org.gimp.GIMP.json"), s, error));
    CHECK(s.downloadSize > 1000000ULL);
    CHECK(s.installedSize > s.downloadSize);
    CHECK(s.arches.contains("x86_64"));
    CHECK(s.hasPermissions);
    CHECK(s.permissions.shared.contains("network"));
    CHECK(s.permissions.filesystems.contains("host"));
    CHECK_EQ(s.permissions.sessionBus["org.freedesktop.FileManager1"], TQString("talk"));

    long long installs = 0;
    CHECK(FlathubJson::parseStats(fixture("stats_org.gimp.GIMP.json"), installs, error));
    CHECK(installs > 1000000LL);
    CHECK(!FlathubJson::parseStats(fixture("stats_missing.json"), installs, error));
    CHECK(installs == -1);

    FpStoreHitList hits;
    CHECK(FlathubJson::parseCollection(fixture("collection_popular.json"), hits, error));
    CHECK(hits.count() == 4);
    CHECK(!hits.isEmpty() && !hits.first().appId.isEmpty() && hits.first().iconUrl.startsWith("https://"));

    FpStoreHitList bad;
    CHECK(!FlathubJson::parseCollection(TQCString("{\"detail\":\"Bad Request\"}"), bad, error));
    CHECK(!FlathubJson::parseCollection(TQCString("not json"), bad, error));
}

static void testAppStream()
{
    const TQString xml = TQString(FIXTURE_DIR) + "/appstream-sample.xml";
    TQString error;
    FpAppList apps;
    CHECK(AppStream::parseFile(xml, "/icons", "flathub", FpScopeUser, "de", apps, error));
    CHECK(apps.count() == 2);   // the runtime is skipped
    if (apps.count() == 2) {
        const FpApp &paint = apps[0];
        CHECK_EQ(paint.id, TQString("org.example.Paint"));
        CHECK_EQ(paint.name, TQString("Malen"));
        CHECK_EQ(paint.developer, TQString("Example Team"));
        CHECK_EQ(paint.license, TQString("GPL-3.0+"));
        CHECK(paint.categories.contains("Graphics") && paint.categories.contains("2DGraphics"));
        CHECK_EQ(paint.iconPath, TQString("/icons/64x64/org.example.Paint.png"));
        CHECK_EQ(paint.appstreamFile, xml);
        const FpApp &game = apps[1];
        CHECK_EQ(game.id, TQString("org.example.Game"));
        CHECK_EQ(game.developer, TQString("Gamers"));
        CHECK(game.categories == TQStringList("Game"));
    }

    FpAppDetails d;
    CHECK(AppStream::parseDetails(xml, "/icons", "org.example.Paint", "", d, error));
    CHECK_EQ(d.name, TQString("Paint"));
    CHECK(d.description.contains("<p>Paint is a <em>simple</em> drawing program &amp; more.</p>"));
    CHECK(d.description.contains("<li>Brushes</li>"));
    CHECK(!d.description.contains("Malen"));
    CHECK(d.screenshots.count() == 1);
    if (d.screenshots.count() == 1) {
        CHECK_EQ(d.screenshots.first().imageUrl, TQString("https://example.org/shot1.png"));
        CHECK_EQ(d.screenshots.first().thumbnailUrl, TQString("https://example.org/shot1-624.png"));
        CHECK_EQ(d.screenshots.first().caption, TQString("Main window"));
    }
    CHECK(d.releases.count() == 2);
    if (d.releases.count() == 2) {
        CHECK_EQ(d.releases.first().version, TQString("2.0"));
        CHECK(d.releases.first().timestamp == 1700000000UL);
        CHECK(d.releases.first().description.contains("Big release."));
    }
    CHECK_EQ(d.urls["homepage"], TQString("https://paint.example.org/"));
    CHECK_EQ(d.iconUrl, TQString("/icons/128x128/org.example.Paint.png"));

    FpAppDetails de;
    CHECK(AppStream::parseDetails(xml, "/icons", "org.example.Paint", "de", de, error));
    CHECK_EQ(de.name, TQString("Malen"));
    CHECK(de.description.contains("Malen ist ein Zeichenprogramm."));

    FpAppDetails missing;
    CHECK(!AppStream::parseDetails(xml, "/icons", "org.example.Nope", "", missing, error));
}

static void testOverrides()
{
    FpPermissions base;
    base.shared << "network" << "ipc";
    base.filesystems << "home" << "xdg-download:ro";
    base.sessionBus["org.freedesktop.Notifications"] = "talk";

    FpPermissions layer;
    layer.shared << "!network";
    layer.filesystems << "!home" << "xdg-download" << "~/Games:create";
    layer.sessionBus["org.freedesktop.Notifications"] = "none";
    layer.environment["FOO"] = "bar";

    const FpPermissions e = Overrides::merge(base, layer);
    CHECK(!e.shared.contains("network"));
    CHECK(e.shared.contains("ipc"));
    CHECK(!e.filesystems.contains("home"));
    CHECK(e.filesystems.contains("xdg-download"));
    CHECK(!e.filesystems.contains("xdg-download:ro"));
    CHECK(e.filesystems.contains("~/Games:create"));
    CHECK(!e.sessionBus.contains("org.freedesktop.Notifications"));
    CHECK_EQ(e.environment["FOO"], TQString("bar"));

    CHECK_EQ(Overrides::filesystemPath("!home"), TQString("home"));
    CHECK_EQ(Overrides::filesystemPath("~/Games:create"), TQString("~/Games"));
    CHECK_EQ(Overrides::filesystemMode("xdg-download:ro"), TQString("ro"));
    CHECK_EQ(Overrides::filesystemMode("host"), TQString("rw"));

    // round trip through the override file (XDG_DATA_HOME set by main)
    TQString error;
    const TQString file = Overrides::overrideFile("org.example.Paint");
    CHECK(file.endsWith("/flatpak/overrides/org.example.Paint"));
    CHECK(Overrides::saveOverride("org.example.Paint", layer, error));
    CHECK(TQFile::exists(file));

    // unknown keys written by other tools must survive
    {
        TQFile f(file);
        CHECK(f.open(IO_WriteOnly | IO_Append));
        f.writeBlock("\n[Custom]\nkeep=yes\n", 19);
    }
    FpPermissions loaded;
    CHECK(Overrides::loadOverride("org.example.Paint", loaded, error));
    CHECK(loaded.shared == TQStringList("!network"));
    CHECK(loaded.filesystems.count() == 3);
    CHECK_EQ(loaded.sessionBus["org.freedesktop.Notifications"], TQString("none"));
    CHECK_EQ(loaded.environment["FOO"], TQString("bar"));

    FpPermissions smaller;
    smaller.shared << "!network";
    CHECK(Overrides::saveOverride("org.example.Paint", smaller, error));
    TQFile f(file);
    CHECK(f.open(IO_ReadOnly));
    const TQString text = TQString::fromUtf8(f.readAll());
    f.close();
    CHECK(text.contains("shared=!network;"));
    CHECK(text.contains("keep=yes"));
    CHECK(!text.contains("filesystems"));
    CHECK(!text.contains("FOO"));

    // an empty layer with only the foreign key keeps the file ...
    CHECK(Overrides::saveOverride("org.example.Paint", FpPermissions(), error));
    CHECK(TQFile::exists(file));
    // ... reset removes it
    CHECK(Overrides::resetOverride("org.example.Paint", error));
    CHECK(!TQFile::exists(file));

    // an empty layer without foreign keys removes the file
    CHECK(Overrides::saveOverride("org.example.Game", smaller, error));
    CHECK(Overrides::saveOverride("org.example.Game", FpPermissions(), error));
    CHECK(!TQFile::exists(Overrides::overrideFile("org.example.Game")));

    // global overrides
    CHECK(Overrides::overrideFile(TQString::null).endsWith("/flatpak/overrides/global"));
}

static void testPatterns()
{
    TQStringList p;
    p << "org.gimp.GIMP" << "org.kde.*" << "runtime/org.gnome.Platform/x86_64/45";
    CHECK(FlatpakBackend::patternsMatch(p, "app/org.gimp.GIMP/x86_64/stable"));
    CHECK(FlatpakBackend::patternsMatch(p, "app/org.kde.kate/x86_64/stable"));
    CHECK(!FlatpakBackend::patternsMatch(p, "app/org.gimp.GIMPx/x86_64/stable"));
    CHECK(FlatpakBackend::patternsMatch(p, "runtime/org.gnome.Platform/x86_64/45"));
    CHECK(!FlatpakBackend::patternsMatch(p, "runtime/org.gnome.Platform/x86_64/46"));
    CHECK(!FlatpakBackend::patternsMatch(p, "app/org.gnome.Platform/x86_64/45"));
    // id//branch form: an empty arch matches any architecture
    CHECK(FlatpakBackend::patternsMatch(TQStringList("org.foo//beta"), "app/org.foo/aarch64/beta"));
    CHECK(!FlatpakBackend::patternsMatch(TQStringList("org.foo//beta"), "app/org.foo/aarch64/stable"));
}

int main()
{
    // keep the override tests away from the real home directory
    const TQString tmp = TQDir::homeDirPath() + "/.cache/tdeflatpak-unittests";
    TQDir().mkdir(TQDir::homeDirPath() + "/.cache");
    TQDir().mkdir(tmp);
    setenv("XDG_DATA_HOME", TQFile::encodeName(tmp).data(), 1);

    testFlathubJson();
    testAppStream();
    testOverrides();
    testPatterns();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
