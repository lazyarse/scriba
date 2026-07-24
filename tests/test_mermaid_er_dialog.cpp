#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidErDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_er_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidErDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidErDialog dlg;
};

TEST_F(MermaidErDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidErDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("erDiagram"));
}

TEST_F(MermaidErDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("erDiagram"));
    EXPECT_TRUE(diagram.contains("{"));
}
