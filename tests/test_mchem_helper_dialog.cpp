#include <gtest/gtest.h>
#include <QApplication>
#include <QPlainTextEdit>
#include <QRadioButton>
#include "MchemHelperDialog.h"
#include "CssUtils.h"

static int g_argc = 1;
static char g_arg0[] = "test_mchem_helper_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MchemHelperDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MchemHelperDialog dlg;
};

class MchemHelperThemeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }
};

TEST_F(MchemHelperDialogTest, WindowTitle) {
    EXPECT_EQ(dlg.windowTitle().toStdString(), "Insert Chemistry Notation");
}

TEST_F(MchemHelperDialogTest, DefaultModeIsInline) {
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

TEST_F(MchemHelperDialogTest, GeneratedNotationEmptyWhenNoInput) {
    EXPECT_TRUE(dlg.generatedNotation().isEmpty());
}

TEST_F(MchemHelperDialogTest, GeneratedNotationBlockFormat) {
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

    input->setPlainText("H2O");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedNotation().toStdString(), "$$\\ce{H2O}$$");
}

TEST_F(MchemHelperDialogTest, GeneratedNotationInlineFormat) {
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

    input->setPlainText("CO2");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedNotation().toStdString(), "$\\ce{CO2}$");
}

TEST_F(MchemHelperDialogTest, GeneratedNotationTrimsWhitespace) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("  H2O  ");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedNotation().toStdString(), "$\\ce{H2O}$");
}

TEST_F(MchemHelperDialogTest, GeneratedNotationEmptyAfterTrim) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("   ");
    QApplication::processEvents();
    EXPECT_TRUE(dlg.generatedNotation().isEmpty());
}

TEST_F(MchemHelperDialogTest, GeneratedNotationWithCharges) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("Fe^{3+}");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedNotation().toStdString(), "$\\ce{Fe^{3+}}$");
}

TEST_F(MchemHelperDialogTest, GeneratedNotationWithArrow) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    input->setPlainText("A + B -> C");
    QApplication::processEvents();
    EXPECT_EQ(dlg.generatedNotation().toStdString(), "$\\ce{A + B -> C}$");
}

TEST_F(MchemHelperThemeTest, ThemeColorsExtractedFromCss) {
    QString css = "#editor { background-color: #0d1117; color: #c9d1d9; }\n"
                  "body { background-color: #0d1117; color: #c9d1d9; }";
    CssUtils::ThemeColors colors = CssUtils::themeColors(css);
    EXPECT_EQ(colors.background.name().toStdString(), "#0d1117");
    EXPECT_EQ(colors.text.name().toStdString(), "#c9d1d9");
}

TEST_F(MchemHelperThemeTest, ThemeColorsDefaultsOnEmptyCss) {
    CssUtils::ThemeColors colors = CssUtils::themeColors("");
    EXPECT_EQ(colors.background.name().toStdString(), "#ffffff");
    EXPECT_EQ(colors.text.name().toStdString(), "#333333");
}

TEST_F(MchemHelperThemeTest, DialogAcceptsThemeCss) {
    QString css = "#editor { background-color: #1e1e2e; color: #cdd6f4; }\n"
                  "body { background-color: #1e1e2e; color: #cdd6f4; }";
    MchemHelperDialog dlg(css);
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
    EXPECT_TRUE(input->styleSheet().contains("#1e1e2e"));
    EXPECT_TRUE(input->styleSheet().contains("#cdd6f4"));
}
