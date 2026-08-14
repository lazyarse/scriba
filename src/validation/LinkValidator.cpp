// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "LinkValidator.h"

#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QUrl>

namespace {

// RFC 3986 scheme: letter, then letters/digits/+-. ; a `:` after it.
bool hasScheme(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    return re.match(s).hasMatch();
}

// host[:port] portion of a `scheme://host...` URL. Ports are stripped only
// when numeric (so IPv6 addresses without brackets survive untouched).
QString hostOf(const QString &url, const QString &scheme)
{
    const QString rest = url.mid(scheme.length() + 3); // scheme + "://"
    QString host = rest.section(QLatin1Char('/'), 0, 0);
    const int bracket = host.indexOf(QLatin1Char('['));
    const int closeBracket = host.indexOf(QLatin1Char(']'));
    if (bracket == 0 && closeBracket > 0)
        return host.mid(1, closeBracket - 1);
    const int colon = host.lastIndexOf(QLatin1Char(':'));
    if (colon > 0) {
        bool numeric = true;
        for (int i = colon + 1; i < host.size(); ++i) {
            if (!host.at(i).isDigit()) {
                numeric = false;
                break;
            }
        }
        if (numeric)
            host.truncate(colon);
    }
    return host;
}

} // namespace

QString LinkValidator::fileTargetPath(const QString &target)
{
    QString t = target.trimmed();
    if (t.startsWith(QLatin1Char('<')) && t.endsWith(QLatin1Char('>')))
        t = t.mid(1, t.size() - 2);
    // File links may carry a #fragment or ?query after the path.
    const int hash = t.indexOf(QLatin1Char('#'));
    const int query = t.indexOf(QLatin1Char('?'));
    int cut = t.size();
    if (hash >= 0)
        cut = qMin(cut, hash);
    if (query >= 0)
        cut = qMin(cut, query);
    return t.left(cut);
}

bool LinkValidator::isValidHttpUrl(const QString &url)
{
    const QString u = url.trimmed();
    if (u.isEmpty() || u.contains(QChar::Space))
        return false;
    if (!hasScheme(u))
        return false;
    const QString scheme = u.section(QLatin1Char(':'), 0, 0).toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")
        && scheme != QStringLiteral("ftp"))
        return false;
    // The scheme must be followed by "//".
    if (u.size() < scheme.length() + 3
        || u.at(scheme.length() + 1) != QLatin1Char('/')
        || u.at(scheme.length() + 2) != QLatin1Char('/'))
        return false;
    const QString host = hostOf(u, scheme);
    if (host.isEmpty())
        return false;
    if (host.contains(QLatin1Char('.')) || host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
        return true;
    return !QHostAddress(host).isNull();
}

LinkValidator::Status LinkValidator::validateTarget(const QString &target, const QString &baseDir)
{
    QString t = target.trimmed();
    if (t.startsWith(QLatin1Char('<')) && t.endsWith(QLatin1Char('>')))
        t = t.mid(1, t.size() - 2);
    if (t.isEmpty() || t.startsWith(QLatin1Char('#')))
        return Status::Valid; // empty link, or an in-document anchor: nothing to check

    // www.-prefixed targets are the user's shorthand for a website (CommonMark
    // would treat them as relative paths — flagging them as missing files would
    // just be noise), so validate them as URLs.
    if (t.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        return isValidHttpUrl(QStringLiteral("http://") + t) ? Status::Valid
                                                             : Status::MalformedUrl;

    if (hasScheme(t)) {
        const QString scheme = t.section(QLatin1Char(':'), 0, 0).toLower();
        if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("ftp"))
            return isValidHttpUrl(t) ? Status::Valid : Status::MalformedUrl;
        // mailto:, tel:, data:, file:, custom schemes — out of scope.
        return Status::Valid;
    }

    const QString path = fileTargetPath(t);
    if (path.isEmpty())
        return Status::Valid;
    if (path.startsWith(QStringLiteral("~/")))
        return QFileInfo(QDir::homePath() + path.mid(1)).exists() ? Status::Valid
                                                                  : Status::FileNotFound;
    if (QFileInfo(path).isAbsolute())
        return QFileInfo(path).exists() ? Status::Valid : Status::FileNotFound;
    const QString dir = baseDir.isEmpty() ? QDir::currentPath() : baseDir;
    return QFileInfo(QDir(dir), path).exists() ? Status::Valid : Status::FileNotFound;
}

QString LinkValidator::headingSlug(const QString &headingText)
{
    // Mirrors the preview's JS (JsSnippets headingIdJs):
    //   h.textContent.toLowerCase().replace(/[^\w\s-]/g,'')
    //     .replace(/\s+/g,'-').replace(/^-+|-+$/g,'')
    // where JS `\w` is ASCII-only [A-Za-z0-9_].
    QString slug;
    slug.reserve(headingText.size());
    bool pendingSpace = false;
    for (const QChar &c : headingText) {
        const char16_t u = c.unicode();
        if (u >= 'A' && u <= 'Z') {
            if (pendingSpace) {
                slug += QLatin1Char('-');
                pendingSpace = false;
            }
            slug += QChar(u + ('a' - 'A'));
        } else if ((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '_' || u == '-') {
            if (pendingSpace) {
                slug += QLatin1Char('-');
                pendingSpace = false;
            }
            slug += c;
        } else if (c.isSpace()) {
            pendingSpace = true;
        }
    }
    // Trim leading/trailing '-'.
    int first = 0;
    int last = slug.size();
    while (first < last && slug.at(first) == QLatin1Char('-'))
        ++first;
    while (last > first && slug.at(last - 1) == QLatin1Char('-'))
        --last;
    return slug.mid(first, last - first);
}

void LinkValidator::addHeadingSlugs(QSet<QString> &out, const QString &headingText)
{
    const QString base = headingSlug(headingText);
    if (base.isEmpty())
        return;
    QString id = base;
    int n = 1;
    while (out.contains(id))
        id = base + QLatin1Char('-') + QString::number(n++);
    out.insert(id);
}
