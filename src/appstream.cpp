/*
 * tdeflatpak - minimal streaming AppStream (collection XML) parser.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "appstream.h"

#include <tqfile.h>
#include <tqxml.h>
#include <tqcstring.h>

#include <zlib.h>

namespace
{

class Handler : public TQXmlDefaultHandler
{
public:
    Handler(const TQString &iconsDir, const TQString &remote, int scope,
            const TQString &lang, FpAppList &out)
        : m_iconsDir(iconsDir), m_remote(remote), m_scope(scope),
          m_lang(lang), m_out(out), m_depth(0), m_componentDepth(-1),
          m_iconSize(0), m_count(0)
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
            }
            return true;
        }

        // only direct children of <component> are interesting
        if (m_depth != m_componentDepth + 1) {
            m_element = TQString::null;
            return true;
        }

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
        m_app.remote = m_remote;
        m_app.scope = m_scope;
        m_out.append(m_app);
        ++m_count;
    }

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
    int m_count;
};

}

namespace AppStream
{

bool parseFile(const TQString &xmlFile, const TQString &iconsDir,
               const TQString &remote, int scope, const TQString &lang,
               FpAppList &out, TQString &error)
{
    gzFile gz = gzopen(TQFile::encodeName(xmlFile).data(), "rb");
    if (!gz) {
        error = TQString("Cannot open %1").arg(xmlFile);
        return false;
    }

    Handler handler(iconsDir, remote, scope, lang, out);
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
