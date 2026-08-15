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
#include <gtest/gtest.h>

#include <QSettings>
#include <QString>

#include "TestConfig.h"
#include "prefs/Preferences.h"
#include "validation/MdLintRules.h"
#include "validation/MdLintConfig.h"

TEST(MdLintRules, RegistryHasExpectedShape)
{
    const auto &rules = MdLintRules::all();
    // MD002, MD006, MD008, MD015, MD016, MD017, MD057 were never implemented
    // upstream (removed/deprecated); 53 upstream rules + 2 custom rules
    // (MD900 unmatched-footnote, MD901 no-loose-lists).
    ASSERT_EQ(55, rules.size());
    const auto *md013 = MdLintRules::byKey(QStringLiteral("MD013"));
    ASSERT_NE(nullptr, md013);
    EXPECT_EQ(QStringLiteral("line-length"), md013->alias);
    EXPECT_TRUE(md013->tags.contains(QStringLiteral("line_length")));
    // alias and tag lookups work
    EXPECT_EQ(md013, MdLintRules::byKey(QStringLiteral("line-length")));
    EXPECT_NE(nullptr, MdLintRules::byKey(QStringLiteral("whitespace")));
    EXPECT_EQ(nullptr, MdLintRules::byKey(QStringLiteral("MD999")));
    EXPECT_TRUE(MdLintRules::rulesForTag(QStringLiteral("table"))
                .contains(QStringLiteral("MD055")));
}

TEST(MdLintRules, AggressiveFlagMatchesScribaDefaults)
{
    // The 7 scriba default-on rules are the only non-aggressive ones.
    const QStringList on = {QStringLiteral("MD001"), QStringLiteral("MD009"),
                            QStringLiteral("MD012"), QStringLiteral("MD013"),
                            QStringLiteral("MD018"), QStringLiteral("MD024"),
                            QStringLiteral("MD900")};
    for (const auto &rule : MdLintRules::all())
        EXPECT_EQ(on.contains(rule.id), !rule.aggressive) << rule.id.toStdString();
}
TEST(MdLintConfig, DefaultsEnableOnlyScribaCoreSet)
{
    const auto cfg = MdLintConfig::defaults();
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD001")));
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD009")));
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD013")));
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD003")));
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD033")));
    EXPECT_EQ(Severity::Warning, cfg.severity(QStringLiteral("MD013")));
    // scriba line-length default is 120, overriding markdownlint's 80
    EXPECT_EQ(120, cfg.param(QStringLiteral("MD013"), "line_length", 80).toInt());
    // tag lookups resolve to member rules
    EXPECT_TRUE(cfg.enabled(QStringLiteral("headings")));
    EXPECT_FALSE(cfg.enabled(QStringLiteral("html")));
    // default-constructed config is all off
    const MdLintConfig empty;
    EXPECT_FALSE(empty.enabled(QStringLiteral("MD001")));
    EXPECT_FALSE(empty.enabled(QStringLiteral("MD013")));
}

TEST(MdLintConfig, DefaultSeverityLevels)
{
    // Style-level rules default to Warning (populating the issue summary's
    // "Markdown warnings" row out of the box); the rendering/structural ones
    // stay Error.
    const auto cfg = MdLintConfig::defaults();
    for (const char *id : {"MD009", "MD012", "MD013", "MD024"})
        EXPECT_EQ(Severity::Warning, cfg.severity(QLatin1String(id))) << id;
    for (const char *id : {"MD001", "MD018", "MD900"})
        EXPECT_EQ(Severity::Error, cfg.severity(QLatin1String(id))) << id;
    // MD901 is aggressive (off in defaults) but declares a Warning default.
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD901")));
    EXPECT_EQ(Severity::Warning, cfg.severity(QStringLiteral("MD901")));
    // An explicit stored config overrides the default severity.
    const auto stored = MdLintConfig::fromJson(QStringLiteral(R"({"MD013": true})"));
    EXPECT_EQ(Severity::Error, stored.severity(QStringLiteral("MD013")));
}

TEST(MdLintConfig, JsonRoundTrip)
{
    const QString json = QStringLiteral(
        R"({"default": false, "MD013": {"enabled": true, "severity": "warning",)"
        R"("params": {"line_length": 100}}, "no-inline-html": true, "whitespace": false})");
    const auto cfg = MdLintConfig::fromJson(json);
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD013")));
    EXPECT_EQ(Severity::Warning, cfg.severity(QStringLiteral("MD013")));
    EXPECT_EQ(100, cfg.param(QStringLiteral("MD013"), "line_length", 80).toInt());
    // alias key enables a rule
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD033")));
    // tag key disables a whole group (case-insensitive)
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD009")));
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD010")));
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD012")));
    // `default`: false turned everything off; only the explicit keys are on
    EXPECT_FALSE(cfg.enabled(QStringLiteral("MD001")));
    EXPECT_EQ(cfg, MdLintConfig::fromJson(cfg.toJson()));
}

TEST(MdLintConfig, UnknownKeysAreIgnored)
{
    const auto cfg = MdLintConfig::fromJson(QStringLiteral(R"({"MD999": false})"));
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD001")));  // defaults still apply
}

TEST(MdLintConfig, EmptyJsonMeansDefaults)
{
    const auto cfg = MdLintConfig::fromJson(QString());
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD001")));
    EXPECT_TRUE(cfg.enabled(QStringLiteral("MD013")));
    EXPECT_EQ(120, cfg.param(QStringLiteral("MD013"), "line_length", 80).toInt());
}

TEST(MdLintConfig, LegacyStoredBlobMigratesToWarningSeverity)
{
    setupTestConfig();
    QSettings settings;
    settings.clear();
    settings.setValue(Preferences::ConfigVersion, 3);
    settings.setValue(Preferences::MarkdownCheckEnabled, true);
    settings.setValue(Preferences::MarkdownLintConfig,
        QStringLiteral(R"({"MD001":true,"MD009":true,"MD012":true,"MD013":true,"MD018":true,"MD024":true,"MD900":true})"));

    Preferences::migrateSettings(settings);

    const auto cfg = MdLintConfig::fromSettings();
    for (const char *id : {"MD009", "MD012", "MD013", "MD024"})
        EXPECT_EQ(Severity::Warning, cfg.severity(QLatin1String(id))) << id;
    EXPECT_EQ(Severity::Error, cfg.severity(QStringLiteral("MD001")));
    EXPECT_EQ(Severity::Error, cfg.severity(QStringLiteral("MD018")));
    EXPECT_EQ(Severity::Error, cfg.severity(QStringLiteral("MD900")));
}
