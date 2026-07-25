#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidStateDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_state_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidStateDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidStateDialog dlg{QString()};
};

TEST_F(MermaidStateDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidStateDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("stateDiagram"));
}

TEST_F(MermaidStateDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("stateDiagram"));
    EXPECT_TRUE(diagram.contains("-->"));
}
