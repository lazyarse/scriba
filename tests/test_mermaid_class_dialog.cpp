#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidClassDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_class_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidClassDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidClassDialog dlg{QString()};
};

TEST_F(MermaidClassDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidClassDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("classDiagram"));
}

TEST_F(MermaidClassDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("classDiagram"));
    EXPECT_TRUE(diagram.contains("class"));
}
