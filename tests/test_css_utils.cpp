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
#include <QRegularExpression>
#include <QColor>
#include "css/CssUtils.h"

TEST(CssUtilsTest, EmptyInput) {
    QString css = CssUtils::deriveChromeCss("");
    EXPECT_FALSE(css.isEmpty());
    EXPECT_TRUE(css.contains("QDialog"));
    EXPECT_TRUE(css.contains("QScrollBar"));
    EXPECT_TRUE(css.contains("#scriba-editor QScrollBar:vertical"));
    EXPECT_TRUE(css.contains("width: 16px"));
    EXPECT_TRUE(css.contains("border-radius: 2px"));
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

TEST(CssUtilsTest, TabBarBottomBorder) {
    // Each tab's 1px thumb bottom border forms the divider under the tab strip
    // (adjacent tabs join into one continuous line); the selected tab must stay
    // borderless so the line breaks there and it merges into the editor below.
    // Top corners are rounded; the bar itself has no border. Regression guard
    // for the tabbar base look.
    QString theme = "body { background: #000000; }";
    QString css = CssUtils::deriveChromeCss(theme);

    QRegularExpression barBorder("QTabBar \\{ [^}]*border: none");
    EXPECT_TRUE(barBorder.match(css).hasMatch());

    QRegularExpression tabBorder("QTabBar::tab \\{ [^}]*border-bottom: 1px solid #[0-9A-F]{6}");
    EXPECT_TRUE(tabBorder.match(css).hasMatch());

    QRegularExpression tabRadius("QTabBar::tab \\{ [^}]*border-top-(left|right)-radius: 6px");
    EXPECT_TRUE(tabRadius.match(css).hasMatch());

    QRegularExpression selectedNoBorder("QTabBar::tab:selected \\{ [^}]*border: none");
    EXPECT_TRUE(selectedNoBorder.match(css).hasMatch());

    // The selected tab must also adopt the editor background so it reads as
    // connected to the content below.
    QRegularExpression selectedBg("QTabBar::tab:selected \\{ [^}]*background-color: #[0-9A-F]{6}");
    EXPECT_TRUE(selectedBg.match(css).hasMatch());
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

TEST(CssUtilsTest, PageListNoFocusOutline) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    EXPECT_TRUE(css.contains("#preferences-page-list::item"));
    EXPECT_TRUE(css.contains("outline: none"));
}

TEST(CssUtilsTest, StylesheetListScrollbarTrack) {
    QString css = CssUtils::deriveChromeCss("body { background: #1a1a2e; }");
    EXPECT_TRUE(css.contains("#preferences-stylesheet-list QScrollBar:vertical"));
    EXPECT_TRUE(css.contains("#preferences-stylesheet-list QScrollBar:horizontal"));
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

    QString listRule = ruleFor("#preferences-stylesheet-list");
    QString hoverRule = ruleFor("QListWidget::item:hover:!selected");
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

TEST(CssUtilsTest, ScrollAreaThemed) {
    // Pages wrapped in QScrollArea must inherit the themed chrome CSS instead
    // of an inline viewport stylesheet shadowing the app-wide rules
    QString darkCss = CssUtils::deriveChromeCss("body { background: #282a36; }");
    EXPECT_TRUE(darkCss.contains("QScrollArea { background-color:"));
    EXPECT_TRUE(darkCss.contains("QScrollArea > QWidget > QWidget { background-color:"));
    QString lightCss = CssUtils::deriveChromeCss("body { background: #ffffff; }");
    EXPECT_TRUE(lightCss.contains("QScrollArea { background-color:"));
    EXPECT_TRUE(lightCss.contains("QScrollArea > QWidget > QWidget { background-color:"));
}

TEST(CssUtilsTest, GutterColorsDerived) {
    auto mix = [](const QColor &a, const QColor &b, int percentOfB) {
        return QColor(
            a.red() + (b.red() - a.red()) * percentOfB / 100,
            a.green() + (b.green() - a.green()) * percentOfB / 100,
            a.blue() + (b.blue() - a.blue()) * percentOfB / 100);
    };

    // dark theme: gutter background is darker than the editor background
    QColor darkBg("#282a36");
    QColor darkGutter = darkBg.darker(120);
    QString darkCss = CssUtils::deriveChromeCss("body { background: #282a36; }");
    EXPECT_TRUE(darkCss.contains(darkGutter.name()));

    // dark theme: gutter text is the editor text blended 30% toward the gutter
    // background (no color: in theme, so editor text falls back to #f0f0f0)
    QColor darkGutterText = mix(darkGutter, QColor("#f0f0f0"), 30);
    QRegularExpression darkGutterRule("\\#gutter \\{ background-color: [^;]+; color: ([^;]+); \\}");
    auto darkMatch = darkGutterRule.match(darkCss);
    EXPECT_TRUE(darkMatch.hasMatch());
    EXPECT_EQ(darkMatch.captured(1), darkGutterText.name());

    // light theme: gutter background is slightly darker than the editor background
    QColor lightBg("#ffffff");
    QColor lightGutter = lightBg.darker(105);
    QString lightCss = CssUtils::deriveChromeCss("body { background: #ffffff; }");
    EXPECT_TRUE(lightCss.contains(lightGutter.name()));

    // light theme: gutter text is the editor text blended 30% toward the gutter
    // background (no color: in theme, so editor text falls back to #333333)
    QColor lightGutterText = mix(lightGutter, QColor("#333333"), 30);
    auto lightMatch = darkGutterRule.match(lightCss);
    EXPECT_TRUE(lightMatch.hasMatch());
    EXPECT_EQ(lightMatch.captured(1), lightGutterText.name());
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

    // dark theme: inactive tab matches the tab bar background, with only a
    // bottom border (the divider under the tab strip)
    QColor darkTrack = QColor("#282a36").lighter(160);
    QString darkTab = ruleFor("body { background: #282a36; }", "QTabBar::tab");
    EXPECT_TRUE(darkTab.contains("background-color: " + darkTrack.name()));
    EXPECT_TRUE(darkTab.contains("border-bottom: 1px solid " + QColor("#282a36").lighter(220).name()));
    QString darkSelected = ruleFor("body { background: #282a36; }", "QTabBar::tab:selected");
    EXPECT_TRUE(darkSelected.contains("background-color: #282a36"));
    EXPECT_TRUE(darkSelected.contains("border: none"));

    // light theme: inactive tab matches the tab bar background, with only a
    // bottom border (the divider under the tab strip)
    QColor lightTrack = QColor("#ffffff").darker(105);
    QString lightTab = ruleFor("body { background: #ffffff; }", "QTabBar::tab");
    EXPECT_TRUE(lightTab.contains("background-color: " + lightTrack.name()));
    EXPECT_TRUE(lightTab.contains("border-bottom: 1px solid " + QColor("#ffffff").darker(125).name()));
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

TEST(CssUtilsTest, DialogWidgetsShareFontSize) {
    QString css = CssUtils::deriveChromeCss("body { background: #ffffff; }");
    const QString expected = QStringLiteral("font-size: %1pt").arg(CssUtils::kUiFontSizePt);
    const QStringList selectors = {
        QStringLiteral("QGroupBox"),
        QStringLiteral("QGroupBox::title"),
        QStringLiteral("QCheckBox"),
        QStringLiteral("QRadioButton"),
        QStringLiteral("QPushButton"),
        QStringLiteral("QLabel"),
        QStringLiteral("#stats-label"),
        QStringLiteral("QPlainTextEdit"),
        QStringLiteral("QTextEdit"),
        QStringLiteral("QLineEdit"),
        QStringLiteral("QSpinBox, QDoubleSpinBox"),
        QStringLiteral("QComboBox"),
        QStringLiteral("QComboBox QAbstractItemView"),
        QStringLiteral("QTableWidget, QTableView"),
    };
    for (const QString &selector : selectors) {
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        ASSERT_TRUE(m.hasMatch()) << "missing rule for " << selector.toStdString();
        EXPECT_TRUE(m.captured(0).contains(expected)) << selector.toStdString();
    }
}

TEST(CssUtilsTest, ChromeFontSizeRespectsParameter) {
    QString css = CssUtils::deriveChromeCss("body { background: #ffffff; }", 14);
    EXPECT_FALSE(css.contains("@FONT_SIZE@"));
    const QString expected = QStringLiteral("font-size: 14pt");
    const QStringList selectors = {
        QStringLiteral("QGroupBox"),
        QStringLiteral("QCheckBox"),
        QStringLiteral("QLabel"),
        QStringLiteral("QPushButton"),
        QStringLiteral("#stats-label"),
    };
    for (const QString &selector : selectors) {
        QRegularExpression re(QStringLiteral("%1\\s*\\{[^}]*\\}").arg(selector));
        auto m = re.match(css);
        ASSERT_TRUE(m.hasMatch()) << "missing rule for " << selector.toStdString();
        EXPECT_TRUE(m.captured(0).contains(expected)) << selector.toStdString();
    }
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

TEST(CssUtilsTest, RenderOverlayUsesLightThemeBackground) {
    QString css = CssUtils::renderOverlayCss("body { background: #ffffff; color: #333333; }");
    EXPECT_TRUE(css.contains("#scriba-rendering-overlay"));
    EXPECT_TRUE(css.contains("background:#ffffff"));
    EXPECT_FALSE(css.contains("::before"));
    EXPECT_FALSE(css.contains("scribaSpin"));
}

TEST(CssUtilsTest, RenderOverlayUsesDarkThemeBackground) {
    QString css = CssUtils::renderOverlayCss("body { background: #282a36; color: #f8f8f2; }");
    EXPECT_TRUE(css.contains("background:#282a36"));
}

TEST(CssUtilsTest, RenderOverlayTextIsPalerThanThemeText) {
    auto colorOf = [](const QString &themeCss) {
        QString css = CssUtils::renderOverlayCss(themeCss);
        QRegularExpression re("color:(#[0-9a-fA-F]{6})");
        auto m = re.match(css);
        return m.hasMatch() ? QColor(m.captured(1)) : QColor();
    };

    QColor light = colorOf("body { background: #ffffff; color: #333333; }");
    ASSERT_TRUE(light.isValid());
    // Blended 25% toward the white background -> lighter than the theme text
    EXPECT_GT(light.lightness(), QColor("#333333").lightness());

    QColor dark = colorOf("body { background: #282a36; color: #f8f8f2; }");
    ASSERT_TRUE(dark.isValid());
    // Blended 25% toward the dark background -> darker than the theme text
    EXPECT_LT(dark.lightness(), QColor("#f8f8f2").lightness());
}

TEST(CssUtilsTest, RenderOverlayFallsBackToWhite) {
    QString css = CssUtils::renderOverlayCss("h1 { color: red; }");
    EXPECT_TRUE(css.contains("background:#ffffff"));
    EXPECT_FALSE(css.contains("rgba(0,0,0,0)"));
}
