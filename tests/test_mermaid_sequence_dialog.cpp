#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidSequenceDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_sequence_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidSequenceDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidSequenceDialog dlg{QString()};
};

TEST_F(MermaidSequenceDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidSequenceDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("sequenceDiagram"));
}

TEST_F(MermaidSequenceDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("sequenceDiagram"));
    EXPECT_TRUE(diagram.contains("->>") || diagram.contains("-->>"));
}
