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
#include <QTemporaryFile>
#include <QString>

#include "css/CssConfig.h"
#include "prefs/Preferences.h"

class CssConfigTest : public testing::Test {
protected:
    void SetUp() override
    {
        QSettings().clear();
    }
};

TEST_F(CssConfigTest, MissingThemeFileIsCleared)
{
    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, "/nonexistent/theme.css");
    settings.setValue(Preferences::CssFiles, QStringList() << "/nonexistent/theme.css");

    CssConfig config;

    EXPECT_TRUE(config.activeStylesheet().isEmpty());
    EXPECT_TRUE(settings.value(Preferences::ActiveCssFile).toString().isEmpty());
}

TEST_F(CssConfigTest, BundledThemeIsPreserved)
{
    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, ":/themes/github-light.css");

    CssConfig config;

    EXPECT_EQ(config.activeStylesheet(), ":/themes/github-light.css");
}

TEST_F(CssConfigTest, RemovedBundledThemeIsCleared)
{
    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, ":/themes/old-removed-theme.css");

    CssConfig config;

    EXPECT_TRUE(config.activeStylesheet().isEmpty());
}

TEST_F(CssConfigTest, ExistingFileThemeIsPreserved)
{
    QTemporaryFile tmp;
    ASSERT_TRUE(tmp.open());
    tmp.close();

    QSettings settings;
    settings.setValue(Preferences::ActiveCssFile, tmp.fileName());

    CssConfig config;

    EXPECT_EQ(config.activeStylesheet(), tmp.fileName());
}
