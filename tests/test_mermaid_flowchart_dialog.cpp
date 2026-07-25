#include <gtest/gtest.h>
#include <QApplication>
#include <QComboBox>
#include "MermaidFlowchartDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_flowchart_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidFlowchartDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidFlowchartDialog dlg{QString(), nullptr};
};

TEST_F(MermaidFlowchartDialogTest, DefaultDiagramIsNonEmpty)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidFlowchartDialogTest, DefaultDiagramStartsWithFlowchart)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("flowchart"));
}

TEST_F(MermaidFlowchartDialogTest, DefaultDirectionIsTD)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("flowchart TD"));
}

TEST_F(MermaidFlowchartDialogTest, DefaultDiagramContainsNodes)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("A[Start]"));
    EXPECT_TRUE(diagram.contains("B(Process)"));
}

TEST_F(MermaidFlowchartDialogTest, DefaultDiagramContainsEdge)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("A-->B"));
}