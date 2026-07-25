#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidTimelineDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_timeline_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidTimelineDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    MermaidTimelineDialog dlg{QString(), nullptr};
};

TEST_F(MermaidTimelineDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidTimelineDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("timeline"));
}

TEST_F(MermaidTimelineDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = dlg.generatedDiagram();
    EXPECT_TRUE(diagram.contains("timeline"));
    EXPECT_NE(diagram.indexOf('\n'), -1);
}
