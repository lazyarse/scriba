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
#include "StoppardEngine.h"

// Stoppard spans are offsets in UTF-16 code units, which is exactly what
// QString indexes with, so no byte<->char bridge is needed (the old
// HarperEngine's byteOffsetPerChar/byteToChar bridge is gone).
//
// The vendored Engine is stateless and check() is const, so this class carries
// no mutex: a single StoppardEngine can be shared across threads without
// serialisation, unlike the old harper-backed engine.
StoppardEngine::StoppardEngine(const QString &dialect)
    : m_engine(stoppard::Dialect::American)
{
    setDialect(dialect);
}

QList<StoppardEngine::Issue> StoppardEngine::check(const QString &text)
{
    const std::u16string_view view(
        reinterpret_cast<const char16_t *>(text.utf16()), text.size());
    const auto issues = m_engine.check(view);

    QList<Issue> out;
    out.reserve(static_cast<qsizetype>(issues.size()));
    for (const auto &it : issues) {
        Issue issue;
        issue.start = it.start;
        issue.length = it.length;
        issue.message = QString::fromStdU16String(it.message);
        for (const auto &s : it.suggestions)
            issue.suggestions.append({static_cast<Issue::SuggestionKind>(s.kind),
                                      QString::fromStdU16String(s.text)});
        out.append(issue);
    }
    return out;
}

void StoppardEngine::setDialect(const QString &dialect)
{
    if (dialect == QStringLiteral("British"))
        m_engine.setDialect(stoppard::Dialect::British);
    else if (dialect == QStringLiteral("Australian"))
        m_engine.setDialect(stoppard::Dialect::Australian);
    else if (dialect == QStringLiteral("Indian"))
        m_engine.setDialect(stoppard::Dialect::Indian);
    else if (dialect == QStringLiteral("Canadian"))
        m_engine.setDialect(stoppard::Dialect::Canadian);
    else if (dialect == QStringLiteral("New Zealand"))
        m_engine.setDialect(stoppard::Dialect::NewZealand);
    else
        m_engine.setDialect(stoppard::Dialect::American);   // incl. "American"
}