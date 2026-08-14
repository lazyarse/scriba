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
#include <QApplication>
#include <QPlainTextEdit>
#include <QRadioButton>
#include "dialogs/KatexHelperDialog.h"
#include "css/CssUtils.h"

static int g_argc = 1;
static char g_arg0[] = "test_katex_helper_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class KatexHelperDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    KatexHelperDialog dlg;
};

class KatexHelperThemeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }
};

TEST_F(KatexHelperDialogTest, WindowTitle) {
    EXPECT_EQ(dlg.windowTitle().toStdString(), "Insert Equation");
}

TEST_F(KatexHelperDialogTest, DefaultModeIsInline) {
    QList<QRadioButton*> radios = dlg.findChildren<QRadioButton*>();
    ASSERT_GE(radios.size(), 2);

    QRadioButton *inlineBtn = nullptr;
    for (auto *r : radios) {
        if (r->text().contains("Inline")) {
            inlineBtn = r;
            break;
        }
    }
    ASSERT_NE(inlineBtn, nullptr);
    EXPECT_TRUE(inlineBtn->isChecked());
}

TEST_F(KatexHelperDialogTest, GeneratedLatexEmptyWhenNoInput) {
    EXPECT_TRUE(dlg.generatedLatex().isEmpty());
}

TEST_F(KatexHelperDialogTest, GeneratedLatexBlockFormat) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);

    QList<QRadioButton*> radios = dlg.findChildren<QRadioButton*>();
    ASSERT_GE(radios.size(), 2);

    QRadioButton *blockBtn = nullptr;
    for (auto *r : radios) {
        if (r->text().contains("Block")) {
            blockBtn = r;
            break;
        }
    }
    ASSERT_NE(blockBtn, nullptr);
    blockBtn->setChecked(true);

    input->setPlainText("x^2");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedLatex().toStdString(), "$$x^2$$");
}

TEST_F(KatexHelperDialogTest, GeneratedLatexInlineFormat) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);

    QList<QRadioButton*> radios = dlg.findChildren<QRadioButton*>();
    ASSERT_GE(radios.size(), 2);

    QRadioButton *inlineBtn = nullptr;
    for (auto *r : radios) {
        if (r->text().contains("Inline")) {
            inlineBtn = r;
            break;
        }
    }
    ASSERT_NE(inlineBtn, nullptr);
    inlineBtn->setChecked(true);

    input->setPlainText("\\frac{a}{b}");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedLatex().toStdString(), "$\\frac{a}{b}$");
}

TEST_F(KatexHelperDialogTest, GeneratedLatexTrimsWhitespace) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("  x  ");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedLatex().toStdString(), "$x$");
}

TEST_F(KatexHelperDialogTest, GeneratedLatexEmptyAfterTrim) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("   ");
    QApplication::processEvents();
    EXPECT_TRUE(dlg.generatedLatex().isEmpty());
}

TEST_F(KatexHelperDialogTest, InputFieldAcceptsMultiLineLatex) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("\\begin{pmatrix}\na & b \\\\\nc & d\n\\end{pmatrix}");
    QApplication::processEvents();
    QString result = dlg.generatedLatex();
    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(result.startsWith("$"));
    EXPECT_TRUE(result.endsWith("$"));
}

TEST_F(KatexHelperThemeTest, ThemeColorsExtractedFromCss) {
    QString css = "#editor { background-color: #0d1117; color: #c9d1d9; }\n"
                  "body { background-color: #0d1117; color: #c9d1d9; }";
    CssUtils::ThemeColors colors = CssUtils::themeColors(css);
    EXPECT_EQ(colors.background.name().toStdString(), "#0d1117");
    EXPECT_EQ(colors.text.name().toStdString(), "#c9d1d9");
}

TEST_F(KatexHelperThemeTest, ThemeColorsFallbackFromBody) {
    QString css = "body { background-color: #ffffff; color: #333333; }";
    CssUtils::ThemeColors colors = CssUtils::themeColors(css);
    EXPECT_EQ(colors.background.name().toStdString(), "#ffffff");
    EXPECT_EQ(colors.text.name().toStdString(), "#333333");
}

TEST_F(KatexHelperThemeTest, ThemeColorsDefaultsOnEmptyCss) {
    CssUtils::ThemeColors colors = CssUtils::themeColors("");
    EXPECT_EQ(colors.background.name().toStdString(), "#ffffff");
    EXPECT_EQ(colors.text.name().toStdString(), "#333333");
}

TEST_F(KatexHelperThemeTest, DialogAcceptsThemeCss) {
    QString css = "#editor { background-color: #1e1e2e; color: #cdd6f4; }\n"
                  "body { background-color: #1e1e2e; color: #cdd6f4; }";
    KatexHelperDialog dlg(css);
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    EXPECT_TRUE(input->styleSheet().contains("#1e1e2e"));
    EXPECT_TRUE(input->styleSheet().contains("#cdd6f4"));
}
