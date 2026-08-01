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
#include <QStringList>

#include "Preferences.h"

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

    EXPECT_EQ(settings.value(Preferences::ReopenLastSession).toBool(), true);
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
    EXPECT_FALSE(settings.contains(Preferences::ReopenLastSession));
}
