#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidPieDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_pie_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidPieDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidPieDialog dlg;
};

TEST_F(MermaidPieDialogTest, DefaultDiagramIsNonEmpty)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidPieDialogTest, DefaultDiagramStartsWithPie)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("pie"));
}

TEST_F(MermaidPieDialogTest, DefaultDiagramContainsData)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("Alpha"));
    EXPECT_TRUE(diagram.contains("Beta"));
    EXPECT_TRUE(diagram.contains("30"));
    EXPECT_TRUE(diagram.contains("70"));
}

TEST_F(MermaidPieDialogTest, ContainsQuotedLabels)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("\"Alpha\""));
    EXPECT_TRUE(diagram.contains("\"Beta\""));
}

TEST_F(MermaidPieDialogTest, ContainsColonSeparators)
{
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains(" : "));
}