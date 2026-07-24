#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidGanttDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_gantt_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidGanttDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidGanttDialog dlg;
};

TEST_F(MermaidGanttDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidGanttDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("gantt"));
}

TEST_F(MermaidGanttDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("gantt"));
    EXPECT_TRUE(diagram.contains("dateFormat"));
}
