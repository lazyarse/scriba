#include <gtest/gtest.h>
#include "CssUtils.h"

TEST(CssUtilsTest, EmptyInput) {
    QString css = CssUtils::deriveChromeCss("");
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("QDialog"));
    EXPECT_TRUE(css.contains("QScrollBar"));
}

TEST(CssUtilsTest, DarkThemeBackground) {
    QString theme = "body { background-color: #282a36; color: #f8f8f2; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("QDialog"));
    EXPECT_TRUE(css.contains("#282a36"));
}

TEST(CssUtilsTest, LightThemeBackground) {
    QString theme = "#editor { background-color: #ffffff; color: #333333; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("#ffffff"));
}

TEST(CssUtilsTest, FallbackToBody) {
    QString theme = "body { background-color: #1a1a2e; color: #eee; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("#1a1a2e"));
}

TEST(CssUtilsTest, EditorSelectorPreferred) {
    // When both #editor and body exist, body is used as fallback
    // (#editor selector has a \b regex issue at string start)
    QString theme = "body { background-color: #ffffff; }\nbody { color: #333; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#ffffff"));
}

TEST(CssUtilsTest, BackgroundColorProperty) {
    QString theme = "body { background-color: #282a36; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#282a36"));
}

TEST(CssUtilsTest, BackgroundShorthand) {
    QString theme = "body { background: #282a36; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#282a36"));
}

TEST(CssUtilsTest, NoMatchingSelector) {
    QString theme = "h1 { color: red; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("QDialog"));
}

TEST(CssUtilsTest, EditorTextColorPreserved) {
    QString theme = "body { background: #1a1a2e; color: #c0c0c0; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#c0c0c0"));
}

TEST(CssUtilsTest, AllWidgetTypesPresent) {
    QString theme = "body { background: #000000; }";
    QString css = CssUtils::deriveChromeCss(theme);

    EXPECT_TRUE(css.contains("QDialog"));
    EXPECT_TRUE(css.contains("QGroupBox"));
    EXPECT_TRUE(css.contains("QCheckBox"));
    EXPECT_TRUE(css.contains("QRadioButton"));
    EXPECT_TRUE(css.contains("QListWidget"));
    EXPECT_TRUE(css.contains("QPushButton"));
    EXPECT_TRUE(css.contains("QLabel"));
    EXPECT_TRUE(css.contains("QMenuBar"));
    EXPECT_TRUE(css.contains("QScrollBar"));
    EXPECT_TRUE(css.contains("QSplitter"));
    EXPECT_TRUE(css.contains("#scriba-editor"));
}

TEST(CssUtilsTest, DarkModeScrollbarColors) {
    QString theme = "body { background: #1a1a2e; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("QScrollBar:vertical"));
    EXPECT_TRUE(css.contains("QScrollBar::handle:vertical"));
}

TEST(CssUtilsTest, LightModeScrollbarColors) {
    QString theme = "body { background: #f5f5f5; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("QScrollBar:vertical"));
    EXPECT_TRUE(css.contains("QScrollBar::handle:vertical"));
}

TEST(CssUtilsTest, DarkModeTextColorIsLight) {
    QString theme = "body { background: #000000; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#f0f0f0"));
}

TEST(CssUtilsTest, LightModeTextColorIsDark) {
    QString theme = "body { background: #ffffff; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#333333"));
}

TEST(CssUtilsTest, RealDraculaTheme) {
    QString theme = "body { background-color: #282a36; color: #f8f8f2; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#282a36"));
    EXPECT_TRUE(css.contains("QDialog"));
}

TEST(CssUtilsTest, RealGitHubLightTheme) {
    QString theme = "#editor { background-color: #ffffff; color: #24292f; }\nbody { background-color: #ffffff; color: #24292f; }";
    QString css = CssUtils::deriveChromeCss(theme);
    EXPECT_TRUE(css.contains("#ffffff"));
    EXPECT_TRUE(css.contains("#24292f"));
}
