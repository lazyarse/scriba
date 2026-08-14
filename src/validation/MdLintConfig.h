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
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVariant>

enum class Severity { Error, Warning };

// Markdown-lint configuration mirroring markdownlint's `options.config`
// semantics: rule keys (id or alias), tag names and `default` all map to
// bool | "error" | "warning" | {enabled, severity, params}. Evaluation order:
// scriba defaults, then `default`, then keys top-to-bottom (later wins).
// Serialized as JSON into Preferences::MarkdownLintConfig.
class MdLintConfig
{
public:
    bool enabled(const QString &ruleKey) const;
    Severity severity(const QString &ruleKey) const;
    QVariant param(const QString &ruleKey, const char *name, const QVariant &def) const;

    QString toJson() const;
    static MdLintConfig fromJson(const QString &json);
    // Applies a JSON object over the current state (later keys win), same
    // semantics as fromJson's key handling.
    void applyJson(const QJsonObject &obj);
    static MdLintConfig defaults();    // the 7 scriba rules on, rest off
    static MdLintConfig allEnabled();  // every rule on with default params
    static MdLintConfig fromSettings();

    bool operator==(const MdLintConfig &o) const;

private:
    struct Entry {
        bool enabled = false;
        Severity sev = Severity::Error;
        QJsonObject params;
    };
    // Applies one key (rule id/alias/tag/`default`) with the given value.
    void applyKey(const QString &key, const QJsonValue &value, bool asDefault);
    QHash<QString, Entry> m_entries;   // keyed by canonical rule id
};
