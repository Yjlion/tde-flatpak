/*
 * tdeflatpak - minimal streaming AppStream (collection XML) parser.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appstream.h"

#include <tqfile.h>
#include <tqxml.h>
#include <tqcstring.h>
#include <tqstylesheet.h>

#include <zlib.h>

namespace
{

class Handler : public TQXmlDefaultHandler
{
public:
    Handler(const TQString &xmlFile, const TQString &iconsDir, const TQString &remote, int scope,
            const TQString &lang, FpAppList &out)
        : m_xmlFile(xmlFile), m_iconsDir(iconsDir), m_remote(remote), m_scope(scope),
          m_lang(lang), m_out(out), m_depth(0), m_componentDepth(-1),
          m_iconSize(0), m_bundleIsFlatpak(false), m_inCategories(false), m_inDeveloper(false),
          m_count(0)
    {
    }

    int count() const { return m_count; }

    bool startElement(const TQString &, const TQString &localName,
                      const TQString &qName, const TQXmlAttributes &atts)
    {
        ++m_depth;
        const TQString name = localName.isEmpty() ? qName : localName;

        if (m_componentDepth < 0) {
            if (name == "component") {
                m_componentDepth = m_depth;
                m_app = FpApp();
                m_name = m_nameLang = m_summary = m_summaryLang = TQString::null;
                m_bestIcon = TQString::null;
                m_iconSize = 0;
                m_bundleIsFlatpak = false;
                m_developer = m_developerName = TQString::null;
                m_inCategories = m_inDeveloper = false;
            }
            return true;
        }

        // grandchildren: <categories><category>, <developer><name>
        if (m_depth == m_componentDepth + 2) {
            if ((m_inCategories && name == "category") || (m_inDeveloper && name == "name")) {
                m_element = name;
                m_text = TQString::null;
                m_elementLang = TQString::null;
                for (int i = 0; i < atts.length(); ++i) {
                    if (atts.qName(i) == "xml:lang" || atts.localName(i) == "lang") {
                        m_elementLang = atts.value(i);
                    }
                }
            } else {
                m_element = TQString::null;
            }
            return true;
        }

        // otherwise only direct children of <component> are interesting
        if (m_depth != m_componentDepth + 1) {
            m_element = TQString::null;
            return true;
        }
        m_inCategories = (name == "categories");
        m_inDeveloper = (name == "developer");

        m_element = name;
        m_text = TQString::null;
        m_elementLang = TQString::null;
        m_attrType = TQString::null;
        m_attrHeight = 0;

        for (int i = 0; i < atts.length(); ++i) {
            const TQString an = atts.qName(i);
            if (an == "xml:lang" || atts.localName(i) == "lang") {
                m_elementLang = atts.value(i);
            } else if (an == "type") {
                m_attrType = atts.value(i);
            } else if (an == "height") {
                m_attrHeight = atts.value(i).toInt();
            }
        }
        return true;
    }

    bool characters(const TQString &ch)
    {
        if (m_componentDepth >= 0 && !m_element.isEmpty()) {
            m_text += ch;
        }
        return true;
    }

    bool endElement(const TQString &, const TQString &, const TQString &)
    {
        if (m_componentDepth >= 0 && m_depth == m_componentDepth + 2 && !m_element.isEmpty()) {
            const TQString text = m_text.stripWhiteSpace();
            if (m_element == "category" && !text.isEmpty()) {
                m_app.categories.append(text);
            } else if (m_element == "name" && m_elementLang.isEmpty()) {
                m_developer = text;
            }
            m_element = TQString::null;
        }
        if (m_componentDepth >= 0 && m_depth == m_componentDepth + 1) {
            m_inCategories = m_inDeveloper = false;
        }
        if (m_componentDepth >= 0 && m_depth == m_componentDepth + 1 && !m_element.isEmpty()) {
            const TQString text = m_text.stripWhiteSpace();
            if (m_element == "id") {
                m_app.id = text;
            } else if (m_element == "name") {
                if (m_elementLang.isEmpty()) {
                    m_name = text;
                } else if (!m_lang.isEmpty() && langMatches(m_elementLang)) {
                    m_nameLang = text;
                }
            } else if (m_element == "summary") {
                if (m_elementLang.isEmpty()) {
                    m_summary = text;
                } else if (!m_lang.isEmpty() && langMatches(m_elementLang)) {
                    m_summaryLang = text;
                }
            } else if (m_element == "icon") {
                if (m_attrType == "cached" && !text.isEmpty()) {
                    // prefer 64px, then the closest larger, then anything
                    int h = m_attrHeight > 0 ? m_attrHeight : 64;
                    int score = (h == 64) ? 1000 : (h > 64 ? 500 - h : h);
                    if (m_bestIcon.isEmpty() || score > m_iconSize) {
                        m_iconSize = score;
                        m_bestIcon = TQString("%1/%2x%3/%4").arg(m_iconsDir).arg(h).arg(h).arg(text);
                    }
                }
            } else if (m_element == "bundle") {
                if (m_attrType == "flatpak") {
                    m_bundleIsFlatpak = true;
                    m_app.ref = text;
                }
            } else if (m_element == "developer_name") {
                if (m_elementLang.isEmpty()) {
                    m_developerName = text;
                }
            } else if (m_element == "project_license") {
                m_app.license = text;
            }
            m_element = TQString::null;
        }

        if (m_componentDepth >= 0 && m_depth == m_componentDepth) {
            finishComponent();
            m_componentDepth = -1;
        }
        --m_depth;
        return true;
    }

private:
    bool langMatches(const TQString &l) const
    {
        if (l == m_lang) {
            return true;
        }
        // "de_DE" vs "de"
        int sep = l.find('_');
        return sep > 0 && l.left(sep) == m_lang;
    }

    void finishComponent()
    {
        if (!m_bundleIsFlatpak || !m_app.ref.startsWith("app/")) {
            return;
        }
        if (m_app.id.isEmpty()) {
            m_app.id = m_app.ref.section('/', 1, 1);
        }
        // ids in AppStream may carry a ".desktop" suffix
        if (m_app.id.endsWith(".desktop")) {
            m_app.id.truncate(m_app.id.length() - 8);
        }
        m_app.name = !m_nameLang.isEmpty() ? m_nameLang : m_name;
        if (m_app.name.isEmpty()) {
            m_app.name = m_app.id;
        }
        m_app.summary = !m_summaryLang.isEmpty() ? m_summaryLang : m_summary;
        m_app.iconPath = m_bestIcon;
        m_app.developer = !m_developer.isEmpty() ? m_developer : m_developerName;
        m_app.appstreamFile = m_xmlFile;
        m_app.iconsDir = m_iconsDir;
        m_app.remote = m_remote;
        m_app.scope = m_scope;
        m_out.append(m_app);
        ++m_count;
    }

    TQString m_xmlFile;
    TQString m_iconsDir;
    TQString m_remote;
    int m_scope;
    TQString m_lang;
    FpAppList &m_out;

    int m_depth;
    int m_componentDepth;
    FpApp m_app;
    TQString m_element;
    TQString m_elementLang;
    TQString m_attrType;
    int m_attrHeight;
    TQString m_text;
    TQString m_name, m_nameLang, m_summary, m_summaryLang;
    TQString m_bestIcon;
    int m_iconSize;
    bool m_bundleIsFlatpak;
    TQString m_developer, m_developerName;
    bool m_inCategories, m_inDeveloper;
    int m_count;
};

// ---------------------------------------------------------------------------
// Details of a single component
// ---------------------------------------------------------------------------

/**
 * Collects the full information of one component.  Description markup is
 * re-serialised into a small HTML subset; elements carrying an xml:lang
 * attribute that does not match the preferred language are skipped, the
 * untranslated text is used when no translation exists.
 */
class DetailsHandler : public TQXmlDefaultHandler
{
public:
    DetailsHandler(const TQString &id, const TQString &iconsDir, const TQString &lang, FpAppDetails &out)
        : m_wanted(id), m_iconsDir(iconsDir), m_lang(lang), m_out(out), m_found(false),
          m_skipDepth(-1), m_langDepth(-1), m_iconScore(0)
    {
    }

    bool found() const { return m_found; }

    bool startElement(const TQString &, const TQString &localName,
                      const TQString &qName, const TQXmlAttributes &atts)
    {
        const TQString name = localName.isEmpty() ? qName : localName;
        m_path.append(name);
        const int depth = m_path.count();
        m_text = TQString::null;

        if (m_found) {
            return false;   // stop parsing, we have what we need
        }
        if (m_skipDepth >= 0) {
            return true;
        }

        TQString lang, type, width, height, version, timestamp;
        for (int i = 0; i < atts.length(); ++i) {
            const TQString an = atts.qName(i);
            if (an == "xml:lang" || atts.localName(i) == "lang") {
                lang = atts.value(i);
            } else if (an == "type") {
                type = atts.value(i);
            } else if (an == "width") {
                width = atts.value(i);
            } else if (an == "height") {
                height = atts.value(i);
            } else if (an == "version") {
                version = atts.value(i);
            } else if (an == "timestamp") {
                timestamp = atts.value(i);
            }
        }

        if (name == "component" && m_componentDepth() < 0) {
            m_compDepth = depth;
            m_cur = FpAppDetails();
            m_descDefault = m_descLang = TQString::null;
            m_nameDefault = m_nameLang = m_sumDefault = m_sumLang = TQString::null;
            m_developer = TQString::null;
            m_iconScore = 0;
            m_isFlatpakApp = false;
            m_bundleRef = TQString::null;
            return true;
        }
        if (m_componentDepth() < 0) {
            return true;
        }

        const int rel = depth - m_compDepth;   // 1 = child of <component>
        const TQString top = rel >= 1 ? m_path[m_compDepth] : TQString::null;

        // language handling: remember where a translated subtree starts
        if (!lang.isEmpty() && m_langDepth < 0) {
            if (m_lang.isEmpty() || !langMatches(lang)) {
                m_skipDepth = depth;     // ignore this subtree entirely
                return true;
            }
            m_langDepth = depth;
        }

        m_attrType = type;
        if (top == "description" && rel >= 2) {
            appendMarkup("<" + htmlTag(name) + ">");
        } else if (top == "screenshots" && rel == 2 && name == "screenshot") {
            m_shot = FpScreenshot();
            m_shotThumbWidth = 0;
        } else if (top == "screenshots" && rel == 3 && name == "image") {
            m_imageType = type;
            m_imageWidth = width.toInt();
        } else if (top == "releases" && rel == 2 && name == "release") {
            m_release = FpRelease();
            m_release.version = version;
            m_release.timestamp = timestamp.toULong();
        } else if (top == "releases" && rel >= 4 && m_path[m_compDepth + 2] == "description") {
            m_release.description += "<" + htmlTag(name) + ">";
        } else if (top == "icon") {
            m_iconHeight = height.toInt();
        } else if (top == "url") {
            m_urlType = type;
        }
        return true;
    }

    bool characters(const TQString &ch)
    {
        if (m_componentDepth() < 0 || m_found || m_skipDepth >= 0) {
            return true;
        }
        m_text += ch;
        const int rel = m_path.count() - m_compDepth;
        if (rel >= 2 && m_path[m_compDepth] == "description") {
            appendMarkup(TQStyleSheet::escape(ch));
        } else if (rel >= 4 && m_path[m_compDepth] == "releases" && m_path[m_compDepth + 2] == "description") {
            m_release.description += TQStyleSheet::escape(ch);
        }
        return true;
    }

    bool endElement(const TQString &, const TQString &, const TQString &)
    {
        const int depth = m_path.count();
        const TQString name = m_path.last();
        m_path.remove(m_path.fromLast());

        if (m_found) {
            return true;
        }
        if (m_skipDepth >= 0) {
            if (depth == m_skipDepth) {
                m_skipDepth = -1;
            }
            return true;
        }
        const bool translated = m_langDepth >= 0;
        if (depth == m_langDepth) {
            m_langDepth = -1;
        }
        if (m_componentDepth() < 0) {
            return true;
        }

        if (depth == m_compDepth) {
            finishComponent();
            m_compDepth = -1;
            return true;
        }

        const int rel = depth - m_compDepth;
        const TQString top = m_path.count() >= (uint)m_compDepth + 1 ? m_path[m_compDepth] : name;
        const TQString text = m_text.stripWhiteSpace();

        if (rel == 1) {
            if (name == "id") {
                m_cur.id = text;
            } else if (name == "name") {
                (translated ? m_nameLang : m_nameDefault) = text;
            } else if (name == "summary") {
                (translated ? m_sumLang : m_sumDefault) = text;
            } else if (name == "developer_name") {
                if (!translated) {
                    m_developer = text;
                }
            } else if (name == "project_license") {
                m_cur.license = text;
            } else if (name == "url" && !text.isEmpty()) {
                m_cur.urls[m_urlType.isEmpty() ? TQString("homepage") : m_urlType] = text;
            } else if (name == "bundle" && m_attrType == "flatpak") {
                m_bundleRef = text;
                m_isFlatpakApp = text.startsWith("app/");
            } else if (name == "icon" && m_attrType == "cached" && !text.isEmpty()) {
                const int h = m_iconHeight > 0 ? m_iconHeight : 64;
                const int score = (h == 128) ? 1000 : (h > 128 ? 500 - h : h);
                if (m_cur.iconUrl.isEmpty() || score > m_iconScore) {
                    m_iconScore = score;
                    m_cur.iconUrl = TQString("%1/%2x%3/%4").arg(m_iconsDir).arg(h).arg(h).arg(text);
                }
            }
        } else if (top == "description" && rel >= 2) {
            appendMarkup("</" + htmlTag(name) + ">");
        } else if (top == "categories" && rel == 2 && name == "category" && !text.isEmpty()) {
            m_cur.categories.append(text);
        } else if (top == "developer" && rel == 2 && name == "name" && !translated) {
            m_developer = text;
        } else if (top == "screenshots" && rel == 3 && name == "caption" && !text.isEmpty()) {
            m_shot.caption = text;
        } else if (top == "screenshots" && rel == 3 && name == "image" && !text.isEmpty()) {
            if (m_imageType == "source") {
                m_shot.imageUrl = text;
            } else {
                // prefer the thumbnail closest to 624px
                const int score = -TQABS(m_imageWidth - 624);
                if (m_shot.thumbnailUrl.isEmpty() || score > m_shotThumbWidth) {
                    m_shotThumbWidth = score;
                    m_shot.thumbnailUrl = text;
                }
            }
        } else if (top == "screenshots" && rel == 2 && name == "screenshot") {
            if (m_shot.thumbnailUrl.isEmpty()) {
                m_shot.thumbnailUrl = m_shot.imageUrl;
            }
            if (m_shot.imageUrl.isEmpty()) {
                m_shot.imageUrl = m_shot.thumbnailUrl;
            }
            if (!m_shot.imageUrl.isEmpty()) {
                m_cur.screenshots.append(m_shot);
            }
        } else if (top == "releases" && rel >= 4 && m_path.count() > (uint)m_compDepth + 2
                   && m_path[m_compDepth + 2] == "description") {
            m_release.description += "</" + htmlTag(name) + ">";
        } else if (top == "releases" && rel == 2 && name == "release") {
            m_cur.releases.append(m_release);
        }
        return true;
    }

private:
    int m_componentDepth() const { return m_compDepth; }

    bool langMatches(const TQString &l) const
    {
        if (l == m_lang) {
            return true;
        }
        int sep = l.find('_');
        return sep > 0 && l.left(sep) == m_lang;
    }

    static TQString htmlTag(const TQString &name)
    {
        if (name == "p" || name == "ul" || name == "ol" || name == "li" || name == "em" || name == "code") {
            return name;
        }
        return "span";
    }

    void appendMarkup(const TQString &markup)
    {
        TQString &target = (m_langDepth >= 0) ? m_descLang : m_descDefault;
        target += markup;
    }

    void finishComponent()
    {
        TQString id = m_cur.id;
        if (id.endsWith(".desktop")) {
            id.truncate(id.length() - 8);
        }
        TQString refId = m_bundleRef.section('/', 1, 1);
        if (!m_isFlatpakApp || (id != m_wanted && refId != m_wanted)) {
            return;
        }
        m_cur.id = m_wanted;
        m_cur.name = !m_nameLang.isEmpty() ? m_nameLang : m_nameDefault;
        m_cur.summary = !m_sumLang.isEmpty() ? m_sumLang : m_sumDefault;
        m_cur.description = !m_descLang.isEmpty() ? m_descLang : m_descDefault;
        m_cur.developer = m_developer;
        m_out = m_cur;
        m_found = true;
    }

    TQString m_wanted;
    TQString m_iconsDir;
    TQString m_lang;
    FpAppDetails &m_out;
    bool m_found;

    TQStringList m_path;
    int m_compDepth = -1;
    int m_skipDepth;
    int m_langDepth;
    TQString m_text;
    TQString m_attrType;

    FpAppDetails m_cur;
    TQString m_descDefault, m_descLang;
    TQString m_nameDefault, m_nameLang, m_sumDefault, m_sumLang;
    TQString m_developer;
    TQString m_bundleRef;
    bool m_isFlatpakApp = false;
    int m_iconScore;
    int m_iconHeight = 0;
    TQString m_urlType;

    FpScreenshot m_shot;
    int m_shotThumbWidth = 0;
    TQString m_imageType;
    int m_imageWidth = 0;
    FpRelease m_release;
};

/** Feed a (possibly gzip-compressed) XML file to @p handler. */
bool parseXml(const TQString &xmlFile, TQXmlDefaultHandler &handler, TQString &error)
{
    gzFile gz = gzopen(TQFile::encodeName(xmlFile).data(), "rb");
    if (!gz) {
        error = TQString("Cannot open %1").arg(xmlFile);
        return false;
    }

    TQXmlSimpleReader reader;
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);

    TQXmlInputSource source;
    const int chunkSize = 256 * 1024;
    TQByteArray buf(chunkSize);
    bool first = true;
    bool ok = true;

    for (;;) {
        int n = gzread(gz, buf.data(), chunkSize);
        if (n < 0) {
            int errnum = 0;
            error = TQString::fromLocal8Bit(gzerror(gz, &errnum));
            ok = false;
            break;
        }
        if (n == 0) {
            break;
        }
        TQByteArray chunk;
        chunk.duplicate(buf.data(), n);
        source.setData(chunk);
        if (first) {
            ok = reader.parse(&source, true);
            first = false;
        } else {
            ok = reader.parseContinue();
        }
        if (!ok) {
            error = handler.errorString();
            break;
        }
    }
    if (ok && !first) {
        // signal end of data
        source.setData(TQString::null);
        reader.parseContinue();
    }
    gzclose(gz);
    return ok;
}

}

namespace AppStream
{

bool parseFile(const TQString &xmlFile, const TQString &iconsDir,
               const TQString &remote, int scope, const TQString &lang,
               FpAppList &out, TQString &error)
{
    Handler handler(xmlFile, iconsDir, remote, scope, lang, out);
    return parseXml(xmlFile, handler, error);
}

bool parseDetails(const TQString &xmlFile, const TQString &iconsDir, const TQString &id,
                  const TQString &lang, FpAppDetails &out, TQString &error)
{
    DetailsHandler handler(id, iconsDir, lang, out);
    if (!parseXml(xmlFile, handler, error) && !handler.found()) {
        return false;
    }
    if (!handler.found()) {
        error = TQString("%1 not found in %2").arg(id).arg(xmlFile);
        return false;
    }
    return true;
}

}
