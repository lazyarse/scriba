#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidSankeyDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_sankey_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidSankeyDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidSankeyDialog dlg{QString(), nullptr};
};

TEST_F(MermaidSankeyDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidSankeyDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("sankey-beta"));
}

TEST_F(MermaidSankeyDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("sankey-beta"));
    EXPECT_TRUE(diagram.contains(","));
}
