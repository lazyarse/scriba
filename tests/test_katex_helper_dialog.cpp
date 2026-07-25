#include <gtest/gtest.h>
#include <QApplication>
#include <QPlainTextEdit>
#include <QRadioButton>
#include "KatexHelperDialog.h"

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

TEST_F(KatexHelperDialogTest, WindowTitle) {
    EXPECT_EQ(dlg.windowTitle().toStdString(), "Insert Equation");
}

TEST_F(KatexHelperDialogTest, DefaultModeIsBlock) {
    QRadioButton *blockBtn = dlg.findChild<QRadioButton*>();
    ASSERT_NE(blockBtn, nullptr);
    EXPECT_TRUE(blockBtn->isChecked());
}

TEST_F(KatexHelperDialogTest, GeneratedLatexEmptyWhenNoInput) {
    EXPECT_TRUE(dlg.generatedLatex().isEmpty());
}

TEST_F(KatexHelperDialogTest, GeneratedLatexBlockFormat) {
    QPlainTextEdit *input = dlg.findChild<QPlainTextEdit*>();
    ASSERT_NE(input, nullptr);
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
    EXPECT_EQ(dlg.generatedLatex().toStdString(), "$$x$$");
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
    EXPECT_TRUE(result.startsWith("$$"));
    EXPECT_TRUE(result.endsWith("$$"));
}
