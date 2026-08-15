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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStringList>

#include "prefs/Preferences.h"

class SettingsMigrationTest : public testing::Test {
protected:
    void SetUp() override
    {
        QSettings().clear();
    }
};

TEST_F(SettingsMigrationTest, MigratesRenamedKeyAndStampsVersion)
{
    QSettings settings;
    settings.setValue("reopenLastFile", true);

    Preferences::migrateSettings(settings);

    EXPECT_EQ(settings.value(Preferences::ReopenLastCorpus).toBool(), true);
    EXPECT_FALSE(settings.contains("reopenLastFile"));
    EXPECT_EQ(settings.value(Preferences::ConfigVersion).toInt(),
              Preferences::CurrentConfigVersion);
}

TEST_F(SettingsMigrationTest, RemovedOptionsAreDropped)
{
    QSettings settings;
    settings.setValue("darkMode", true);
    settings.setValue("editorOnLeft", false);
    settings.setValue("showFoldIcons", true);
    settings.setValue("firstRun", true);
    settings.setValue("printCssFiles", QStringList() << "/tmp/x.css");
    settings.setValue("activePrintCssFile", "/tmp/x.css");
    settings.setValue("cssDirectory", "/tmp/css");
    settings.setValue("enabledCssFiles", QStringList() << "a");
    settings.setValue("EditorFont", "Arial");
    settings.setValue("editorFont", "Arial");

    Preferences::migrateSettings(settings);

    for (const QString &key : {QStringLiteral("darkMode"), QStringLiteral("editorOnLeft"),
                               QStringLiteral("showFoldIcons"), QStringLiteral("firstRun"),
                               QStringLiteral("printCssFiles"), QStringLiteral("activePrintCssFile"),
                               QStringLiteral("cssDirectory"), QStringLiteral("enabledCssFiles"),
                               QStringLiteral("EditorFont"), QStringLiteral("editorFont")})
        EXPECT_FALSE(settings.contains(key)) << key.toStdString();
}

TEST_F(SettingsMigrationTest, LegacySessionKeysAreDroppedNotPorted)
{
    QSettings settings;
    settings.setValue("reopenLastSession", true);
    settings.setValue("sessionData", "{}");
    settings.setValue("lastSessionName", "x");

    Preferences::migrateSettings(settings);

    EXPECT_FALSE(settings.contains("reopenLastSession"));
    EXPECT_FALSE(settings.contains("sessionData"));
    EXPECT_FALSE(settings.contains("lastSessionName"));
    EXPECT_FALSE(settings.contains("reopenLastCorpus"));
    EXPECT_FALSE(settings.contains("onExitCorpusData"));
}

TEST_F(SettingsMigrationTest, UnknownKeysArePreserved)
{
    QSettings settings;
    settings.setValue("someFutureOption", 42);

    Preferences::migrateSettings(settings);

    EXPECT_EQ(settings.value("someFutureOption").toInt(), 42);
    EXPECT_EQ(settings.value(Preferences::ConfigVersion).toInt(),
              Preferences::CurrentConfigVersion);
}

TEST_F(SettingsMigrationTest, AlreadyMigratedConfigIsUntouched)
{
    QSettings settings;
    settings.setValue(Preferences::ConfigVersion, Preferences::CurrentConfigVersion);
    settings.setValue("reopenLastFile", true);

    Preferences::migrateSettings(settings);

    EXPECT_TRUE(settings.contains("reopenLastFile"));
    EXPECT_FALSE(settings.contains(Preferences::ReopenLastCorpus));
}

TEST_F(SettingsMigrationTest, PreviewRenderDelayDefaultsMatchDebounceConstants)
{
    EXPECT_EQ(Preferences::DefaultPreviewUpdateDelay, Debounce::LightRender);
    EXPECT_EQ(Preferences::DefaultHeavyRenderDelay, Debounce::HeavyRender);
}

TEST_F(SettingsMigrationTest, PreviewUpdateDelayKeyRoundTrips)
{
    QSettings settings;
    settings.setValue(Preferences::PreviewUpdateDelay, 240);

    EXPECT_EQ(settings.value(Preferences::PreviewUpdateDelay,
              Preferences::DefaultPreviewUpdateDelay).toInt(), 240);
}

TEST_F(SettingsMigrationTest, PreviewUpdateDelayDefaultsToDebounceConstant)
{
    QSettings settings;
    settings.clear();

    EXPECT_EQ(settings.value(Preferences::PreviewUpdateDelay,
              Preferences::DefaultPreviewUpdateDelay).toInt(),
              Debounce::LightRender);
}

TEST_F(SettingsMigrationTest, LegacyLintConfigGetsWarningSeverities)
{
    QSettings settings;
    settings.setValue(Preferences::ConfigVersion, 3);
    settings.setValue(Preferences::MarkdownLintConfig,
        QStringLiteral(R"({"MD001":true,"MD009":true,"MD012":true,"MD013":true,"MD018":true,"MD024":true,"MD900":true})"));

    Preferences::migrateSettings(settings);

    const QJsonObject obj = QJsonDocument::fromJson(
        settings.value(Preferences::MarkdownLintConfig).toString().toUtf8()).object();
    for (const char *id : {"MD009", "MD012", "MD013", "MD024"})
        EXPECT_EQ(QJsonValue(QStringLiteral("warning")), obj.value(QLatin1String(id))) << id;
    for (const char *id : {"MD001", "MD018", "MD900"})
        EXPECT_EQ(QJsonValue(true), obj.value(QLatin1String(id))) << id;
    EXPECT_EQ(settings.value(Preferences::ConfigVersion).toInt(), 4);
}

TEST_F(SettingsMigrationTest, ExplicitAndUnknownLintEntriesUntouched)
{
    QSettings settings;
    settings.setValue(Preferences::ConfigVersion, 3);
    const QString blob = QStringLiteral(
        R"({"MD013": "warning", "MD001": {"enabled": true, "params": {"style": "consistent"}}, "MD007": false, "CUSTOM_KEY": true})");
    settings.setValue(Preferences::MarkdownLintConfig, blob);

    Preferences::migrateSettings(settings);

    // None of these entries is a bare-`true` Warning-by-default rule, so the
    // blob must survive byte-identical (explicit strings/objects, `false`,
    // and unknown keys all pass through).
    EXPECT_EQ(blob, settings.value(Preferences::MarkdownLintConfig).toString());
}

TEST_F(SettingsMigrationTest, CurrentVersionConfigUnchanged)
{
    QSettings settings;
    settings.setValue(Preferences::ConfigVersion, Preferences::CurrentConfigVersion);
    settings.setValue(Preferences::MarkdownLintConfig,
        QStringLiteral(R"({"MD001":true,"MD009":true,"MD012":true,"MD013":true,"MD018":true,"MD024":true,"MD900":true})"));

    Preferences::migrateSettings(settings);

    EXPECT_EQ(QStringLiteral(R"({"MD001":true,"MD009":true,"MD012":true,"MD013":true,"MD018":true,"MD024":true,"MD900":true})"),
              settings.value(Preferences::MarkdownLintConfig).toString());
}
