#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidJourneyDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_journey_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidJourneyDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidJourneyDialog dlg{QString(), nullptr};
};

TEST_F(MermaidJourneyDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidJourneyDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("journey"));
}

TEST_F(MermaidJourneyDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("journey"));
    EXPECT_NE(diagram.indexOf('\n'), -1);
}
