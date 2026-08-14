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
#include "MdLintConfig.h"

#include "MdLintRules.h"
#include "prefs/Preferences.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSettings>

namespace {

// Resolves a config key to canonical rule ids. `default` yields nothing (the
// caller treats it specially); a rule id/alias yields that rule; a tag name
// expands to every member rule; anything else yields nothing.
QStringList canonicalRuleIds(const QString &key)
{
    const QString k = key.toLower();
    if (k == QLatin1String("default"))
        return {};
    for (const auto &r : MdLintRules::all())
        if (r.id.compare(k, Qt::CaseInsensitive) == 0
            || r.alias.compare(k, Qt::CaseInsensitive) == 0)
            return {r.id};
    return MdLintRules::rulesForTag(key);
}

} // namespace

void MdLintConfig::applyKey(const QString &key, const QJsonValue &value, bool asDefault)
{
    const QStringList ids = canonicalRuleIds(key);
    if (ids.isEmpty() && !asDefault)
        return;  // unknown key — ignore

    Entry base;
    if (value.isBool()) {
        base.enabled = value.toBool();
    } else if (value.isString()) {
        const QString s = value.toString().toLower();
        base.enabled = s == QLatin1String("error") || s == QLatin1String("warning");
        if (s == QLatin1String("warning"))
            base.sev = Severity::Warning;
    } else if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        base.enabled = obj.value(QLatin1String("enabled")).toBool(true);
        const QString sev = obj.value(QLatin1String("severity")).toString().toLower();
        if (sev == QLatin1String("warning"))
            base.sev = Severity::Warning;
        base.params = obj.value(QLatin1String("params")).toObject();
    } else {
        return;
    }

    if (asDefault) {
        for (const auto &r : MdLintRules::all())
            m_entries.insert(r.id, base);
        return;
    }
    if (ids.isEmpty())
        return;
    for (const auto &id : ids)
        m_entries.insert(id, base);
}

bool MdLintConfig::enabled(const QString &ruleKey) const
{
    const QStringList ids = canonicalRuleIds(ruleKey);
    for (const auto &id : ids) {
        const auto it = m_entries.constFind(id);
        if (it != m_entries.constEnd() && it->enabled)
            return true;
    }
    return false;
}

Severity MdLintConfig::severity(const QString &ruleKey) const
{
    const QStringList ids = canonicalRuleIds(ruleKey);
    for (const auto &id : ids) {
        const auto it = m_entries.constFind(id);
        if (it != m_entries.constEnd())
            return it->sev;
    }
    return Severity::Error;
}

QVariant MdLintConfig::param(const QString &ruleKey, const char *name, const QVariant &def) const
{
    const QStringList ids = canonicalRuleIds(ruleKey);
    for (const auto &id : ids) {
        const auto it = m_entries.constFind(id);
        if (it == m_entries.constEnd())
            continue;
        const QJsonValue v = it->params.value(QLatin1String(name));
        if (v.isUndefined())
            continue;
        switch (v.type()) {
        case QJsonValue::Bool:   return v.toBool();
        case QJsonValue::Double: return v.toInt();
        case QJsonValue::String: return v.toString();
        case QJsonValue::Array: {
            QStringList list;
            for (const auto &e : v.toArray())
                list << e.toString();
            return list;
        }
        default: continue;
        }
    }
    return def;
}

QString MdLintConfig::toJson() const
{
    QJsonObject root;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const Entry &e = it.value();
        if (e.params.isEmpty()) {
            if (e.enabled)
                root.insert(it.key(), e.sev == Severity::Warning
                                          ? QJsonValue(QLatin1String("warning"))
                                          : QJsonValue(true));
            else
                root.insert(it.key(), false);
        } else {
            QJsonObject obj;
            obj.insert(QLatin1String("enabled"), e.enabled);
            if (e.sev == Severity::Warning)
                obj.insert(QLatin1String("severity"), QLatin1String("warning"));
            obj.insert(QLatin1String("params"), e.params);
            root.insert(it.key(), obj);
        }
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

MdLintConfig MdLintConfig::fromJson(const QString &json)
{
    MdLintConfig cfg;
    // Seed with the scriba defaults, then apply the JSON document.
    cfg = defaults();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return cfg;
    const QJsonObject root = doc.object();
    const QJsonValue def = root.value(QLatin1String("default"));
    if (!def.isUndefined())
        cfg.applyKey(QLatin1String("default"), def, true);
    for (auto it = root.begin(); it != root.end(); ++it)
        if (it.key().compare(QLatin1String("default"), Qt::CaseInsensitive) != 0)
            cfg.applyKey(it.key(), it.value(), false);
    return cfg;
}

MdLintConfig MdLintConfig::defaults()
{
    MdLintConfig cfg;
    // Start from the scriba default-on set (the 7 non-aggressive rules) with
    // the scriba line-length default of 120, everything else off.
    const auto seed = allEnabled();
    for (const auto &r : MdLintRules::all()) {
        if (r.aggressive)
            continue;
        cfg.m_entries.insert(r.id, seed.m_entries.value(r.id));
    }
    cfg.m_entries[QLatin1String("MD013")].params.insert(QLatin1String("line_length"), 120);
    // Parity with the legacy MarkdownChecker: any trailing whitespace is
    // flagged (br_spaces 0) and only runs of 3+ blank lines are reported
    // (maximum 2), where markdownlint's own defaults are 2 and 1.
    cfg.m_entries[QLatin1String("MD009")].params.insert(QLatin1String("br_spaces"), 0);
    cfg.m_entries[QLatin1String("MD012")].params.insert(QLatin1String("maximum"), 2);
    return cfg;
}

MdLintConfig MdLintConfig::allEnabled()
{
    MdLintConfig cfg;
    for (const auto &r : MdLintRules::all()) {
        Entry e;
        e.enabled = true;
        for (const auto &p : MdLintRules::paramsFor(r.id))
            e.params.insert(QLatin1String(p.name), QJsonValue::fromVariant(p.def));
        cfg.m_entries.insert(r.id, e);
    }
    return cfg;
}

MdLintConfig MdLintConfig::fromSettings()
{
    QSettings settings;
    if (!settings.value(Preferences::MarkdownCheckEnabled, false).toBool())
        return {};
    return fromJson(settings.value(Preferences::MarkdownLintConfig).toString());
}

bool MdLintConfig::operator==(const MdLintConfig &o) const
{
    if (m_entries.size() != o.m_entries.size())
        return false;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const auto oit = o.m_entries.constFind(it.key());
        if (oit == o.m_entries.constEnd())
            return false;
        if (it->enabled != oit->enabled || it->sev != oit->sev || it->params != oit->params)
            return false;
    }
    return true;
}