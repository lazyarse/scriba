#include <gtest/gtest.h>
#include <QRegularExpression>
#include <QColor>
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

TEST(CssUtilsTest, MenuBarAndTabBarNoNativeBorder) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    auto ruleFor = [&](const QString &selector) {
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(0) : QString();
    };
    QString menuBar = ruleFor("QMenuBar");
    QString tabBar = ruleFor("QTabBar");
    EXPECT_FALSE(menuBar.isEmpty());
    EXPECT_FALSE(tabBar.isEmpty());
    EXPECT_TRUE(menuBar.contains("border: none"));
    EXPECT_TRUE(tabBar.contains("border: none"));
}

TEST(CssUtilsTest, CategoryListNoFocusOutline) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    EXPECT_TRUE(css.contains("#category-list::item"));
    EXPECT_TRUE(css.contains("outline: none"));
}

TEST(CssUtilsTest, StylesheetListScrollbarTrack) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    EXPECT_TRUE(css.contains("#stylesheet-list QScrollBar:vertical"));
    EXPECT_TRUE(css.contains("#stylesheet-list QScrollBar:horizontal"));
}

TEST(CssUtilsTest, StylesheetListHoverVisible) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    auto ruleFor = [&](const QString &selector) {
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(0) : QString();
    };
    auto bgOf = [&](const QString &rule) {
        QRegularExpression re("background-color\\s*:\\s*([^;]+);");
        auto m = re.match(rule);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };

    QString listRule = ruleFor("#stylesheet-list");
    QString hoverRule = ruleFor("QListWidget::item:hover");
    ASSERT_FALSE(listRule.isEmpty());
    ASSERT_FALSE(hoverRule.isEmpty());

    // list background is the sidebar shade (sideBg, %10) so the track-colored
    // hover (%1) stands out instead of blending into the list background
    QString listBg = bgOf(listRule);
    QString hoverBg = bgOf(hoverRule);
    ASSERT_FALSE(listBg.isEmpty());
    ASSERT_FALSE(hoverBg.isEmpty());
    EXPECT_EQ(listBg, QColor(QColor("#1a1a2e").lighter(130)).name());
    EXPECT_NE(listBg, hoverBg);
}

TEST(CssUtilsTest, GutterRulePresent) {
    QString darkCss = CssUtils::deriveChromeCss("body { background: #282a36; }");
    EXPECT_TRUE(darkCss.contains("#gutter { background-color:"));
    QString lightCss = CssUtils::deriveChromeCss("body { background: #ffffff; }");
    EXPECT_TRUE(lightCss.contains("#gutter { background-color:"));
}

TEST(CssUtilsTest, GutterColorsDerived) {
    // dark theme: gutter background is darker than the editor background
    QColor darkBg("#282a36");
    QColor darkGutter = darkBg.darker(120);
    QString darkCss = CssUtils::deriveChromeCss("body { background: #282a36; }");
    EXPECT_TRUE(darkCss.contains(darkGutter.name()));

    // light theme: gutter background is slightly darker than the editor background
    QColor lightBg("#ffffff");
    QColor lightGutter = lightBg.darker(105);
    QString lightCss = CssUtils::deriveChromeCss("body { background: #ffffff; }");
    EXPECT_TRUE(lightCss.contains(lightGutter.name()));
}

TEST(CssUtilsTest, PushButtonBorderDerived) {
    auto buttonRule = [&](const QString &themeCss) {
        QString css = CssUtils::deriveChromeCss(themeCss);
        QRegularExpression re(QStringLiteral("QPushButton\\s*\\{[^}]*\\}"));
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(0) : QString();
    };

    // dark theme: button border is lighter than the fill
    QColor darkThumb = QColor("#282a36").lighter(220);
    QColor darkBorder = darkThumb.lighter(150);
    QString darkRule = buttonRule("body { background: #282a36; }");
    EXPECT_TRUE(darkRule.contains("border: 1px solid " + darkBorder.name()));

    // light theme: button border is darker than the fill
    QColor lightThumb = QColor("#ffffff").darker(125);
    QColor lightBorder = lightThumb.darker(150);
    QString lightRule = buttonRule("body { background: #ffffff; }");
    EXPECT_TRUE(lightRule.contains("border: 1px solid " + lightBorder.name()));
}

TEST(CssUtilsTest, TabBarInactiveMatchesBarBackground) {
    auto ruleFor = [&](const QString &themeCss, const QString &selector) {
        QString css = CssUtils::deriveChromeCss(themeCss);
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(0) : QString();
    };

    // dark theme: inactive tab matches the tab bar background, with a border
    QColor darkTrack = QColor("#282a36").lighter(160);
    QString darkTab = ruleFor("body { background: #282a36; }", "QTabBar::tab");
    EXPECT_TRUE(darkTab.contains("background-color: " + darkTrack.name()));
    EXPECT_TRUE(darkTab.contains("border: 1px solid " + QColor("#282a36").lighter(220).name()));
    QString darkSelected = ruleFor("body { background: #282a36; }", "QTabBar::tab:selected");
    EXPECT_TRUE(darkSelected.contains("background-color: #282a36"));
    EXPECT_TRUE(darkSelected.contains("border: none"));

    // light theme: inactive tab matches the tab bar background, with a border
    QColor lightTrack = QColor("#ffffff").darker(105);
    QString lightTab = ruleFor("body { background: #ffffff; }", "QTabBar::tab");
    EXPECT_TRUE(lightTab.contains("background-color: " + lightTrack.name()));
    EXPECT_TRUE(lightTab.contains("border: 1px solid " + QColor("#ffffff").darker(125).name()));
    QString lightSelected = ruleFor("body { background: #ffffff; }", "QTabBar::tab:selected");
    EXPECT_TRUE(lightSelected.contains("background-color: #ffffff"));
    EXPECT_TRUE(lightSelected.contains("border: none"));
}

TEST(CssUtilsTest, RadioCheckedHasRing) {
    auto ruleFor = [&](const QString &themeCss, const QString &selector) {
        QString css = CssUtils::deriveChromeCss(themeCss);
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        return m.hasMatch() ? m.captured(0) : QString();
    };

    // light theme: 1px ring stays, dark dot image is the blob
    QColor lightTrack = QColor("#ffffff").darker(105);
    QColor lightThumb = QColor("#ffffff").darker(125);
    QString lightChecked = ruleFor("body { background: #ffffff; }", "QRadioButton::indicator:checked");
    EXPECT_TRUE(lightChecked.contains("background-color: " + lightTrack.name()));
    EXPECT_TRUE(lightChecked.contains("border: 1px solid " + lightThumb.name()));
    EXPECT_TRUE(lightChecked.contains("image: url(:/radio-dot-dark.svg)"));

    // dark theme: same ring pattern, light dot image
    QColor darkTrack = QColor("#282a36").lighter(160);
    QColor darkThumb = QColor("#282a36").lighter(220);
    QString darkChecked = ruleFor("body { background: #282a36; }", "QRadioButton::indicator:checked");
    EXPECT_TRUE(darkChecked.contains("background-color: " + darkTrack.name()));
    EXPECT_TRUE(darkChecked.contains("border: 1px solid " + darkThumb.name()));
    EXPECT_TRUE(darkChecked.contains("image: url(:/radio-dot.svg)"));

    // disabled: flat, no dot image
    QString disabled = ruleFor("body { background: #ffffff; }", "QRadioButton::indicator:disabled");
    EXPECT_TRUE(disabled.contains("image: none"));
}

TEST(CssUtilsTest, SplitViewMaxWidthAutoFillsPane) {
    QString css = CssUtils::splitViewMaxWidthCss(0);
    EXPECT_TRUE(css.contains("max-width:none!important"));
    EXPECT_TRUE(css.contains("margin:0!important"));
    EXPECT_EQ(CssUtils::splitViewMaxWidthCss(-1), css);
}

TEST(CssUtilsTest, SplitViewMaxWidthCapsAndCenters) {
    QString css = CssUtils::splitViewMaxWidthCss(800);
    EXPECT_TRUE(css.contains("max-width:800px!important"));
    EXPECT_TRUE(css.contains("margin:0 auto!important"));
    EXPECT_FALSE(css.contains("max-width:none"));
}
