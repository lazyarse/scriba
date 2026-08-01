#include <gtest/gtest.h>
#include <QSettings>
#include <QTemporaryFile>
#include <QString>

#include "CssConfig.h"
#include "Preferences.h"

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
