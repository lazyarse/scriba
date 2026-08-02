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
#include "HarperEngine.h"
#include "Preferences.h"
#include <QByteArray>
#include <QSettings>
#include <QVector>
#include <cstddef>

// Plain C ABI from vendor/harper-ffi/src/lib.rs. usize = size_t.
extern "C" {
    void *harper_init(unsigned char dialect_code);
    void *harper_lint(void *engine, const unsigned char *text, size_t text_len);
    size_t harper_issues_len(const void *list);
    size_t harper_issue_start(const void *list, size_t i);
    size_t harper_issue_len(const void *list, size_t i);
    const unsigned char *harper_issue_message(const void *list, size_t i, size_t *out_len);
    void harper_free_issues(void *list);
    void harper_free(void *engine);
}

namespace {

// Maps UTF-8 byte offsets (as returned by the FFI) back to QString character
// offsets. UTF-16 surrogate pairs are treated as a single logical position.
QVector<int> byteOffsetPerChar(const QString &text)
{
    QVector<int> offsets;
    offsets.reserve(text.size() + 1);
    int bytes = 0;
    const ushort *data = text.utf16();
    const int len = text.size();
    for (int i = 0; i < len; ++i) {
        offsets.append(bytes);
        uint cp = data[i];
        if (QChar::isHighSurrogate(cp) && i + 1 < len && QChar::isLowSurrogate(data[i + 1]))
            cp = QChar::surrogateToUcs4(cp, data[++i]);
        if (cp < 0x80)
            bytes += 1;
        else if (cp < 0x800)
            bytes += 2;
        else if (cp < 0x10000)
            bytes += 3;
        else
            bytes += 4;
    }
    offsets.append(bytes);
    return offsets;
}

int byteToChar(const QVector<int> &offsets, int byte)
{
    const int n = offsets.size();
    int lo = 0, hi = n;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (offsets[mid] <= byte)
            lo = mid + 1;
        else
            hi = mid;
    }
    return qMax(0, lo - 1);
}

} // namespace

HarperEngine::HarperEngine()
{
    // An empty m_dialect forces the first setDialect() call (below) to build
    // the engine, so the initial engine already matches the saved preference.
    QSettings settings;
    setDialect(settings.value(Preferences::HarperDialect, QStringLiteral("American")).toString());
}

HarperEngine::~HarperEngine()
{
    QMutexLocker locker(&m_mutex);
    if (m_engine)
        harper_free(m_engine);
}

void HarperEngine::setDialect(const QString &dialect)
{
    if (dialect == m_dialect)
        return;

    unsigned char code = 0; // American (fallback for unknown names)
    if (dialect == QStringLiteral("British"))
        code = 1;
    else if (dialect == QStringLiteral("Australian"))
        code = 2;
    else if (dialect == QStringLiteral("Indian"))
        code = 3;
    else if (dialect == QStringLiteral("Canadian"))
        code = 4;

    QMutexLocker locker(&m_mutex);
    void *engine = harper_init(code);
    if (!engine)
        return; // keep the old engine when the rebuild fails
    if (m_engine)
        harper_free(m_engine);
    m_engine = engine;
    m_dialect = dialect;
}

QList<GrammarChecker::Issue> HarperEngine::check(const QString &text)
{
    QList<Issue> issues;
    if (!m_engine)
        return issues;

    const QByteArray utf8 = text.toUtf8();
    void *list;
    {
        QMutexLocker locker(&m_mutex);
        list = harper_lint(m_engine,
                           reinterpret_cast<const unsigned char *>(utf8.constData()),
                           size_t(utf8.size()));
    }
    if (!list)
        return issues;

    const QVector<int> charOffsets = byteOffsetPerChar(text);
    const int lastChar = qMax(0, charOffsets.size() - 2); // text.size() clamped

    const size_t count = harper_issues_len(list);
    issues.reserve(int(count));
    for (size_t i = 0; i < count; ++i) {
        size_t outLen = 0;
        const unsigned char *msgPtr = harper_issue_message(list, i, &outLen);
        const QString message = msgPtr
            ? QString::fromUtf8(reinterpret_cast<const char *>(msgPtr), int(outLen))
            : QString();

        const int startByte = int(harper_issue_start(list, i));
        const int endByte = startByte + int(harper_issue_len(list, i));
        const int start = qMin(byteToChar(charOffsets, startByte), lastChar);
        const int end = qMin(byteToChar(charOffsets, endByte), lastChar);
        if (end <= start)
            continue;
        issues.append({start, end - start, message});
    }
    harper_free_issues(list);
    return issues;
}
