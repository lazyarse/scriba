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
