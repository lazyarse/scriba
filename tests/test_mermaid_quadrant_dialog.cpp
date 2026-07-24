#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidQuadrantDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_quadrant_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidQuadrantDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidQuadrantDialog dlg;
};

TEST_F(MermaidQuadrantDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidQuadrantDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("quadrantChart"));
}

TEST_F(MermaidQuadrantDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("quadrantChart"));
    EXPECT_TRUE(diagram.contains("x-axis"));
}
