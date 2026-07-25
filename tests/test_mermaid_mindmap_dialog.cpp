#include <gtest/gtest.h>
#include <QApplication>
#include "MermaidMindmapDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_mindmap_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class MermaidMindmapDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
        s_dlg = new MermaidMindmapDialog;
    }

    static void TearDownTestSuite() {
        // WebEngine background threads race during explicit delete.
        // Leak intentionally — process reclaims memory instantly on exit.
        s_dlg = nullptr;
    }

    static MermaidMindmapDialog *s_dlg;
};

MermaidMindmapDialog *MermaidMindmapDialogTest::s_dlg = nullptr;

TEST_F(MermaidMindmapDialogTest, DefaultDiagramIsNonEmpty) {
    QString diagram = s_dlg->generatedDiagram();
    EXPECT_FALSE(diagram.isEmpty());
}

TEST_F(MermaidMindmapDialogTest, DefaultDiagramStartsWithExpectedKeyword) {
    QString diagram = s_dlg->generatedDiagram();
    EXPECT_TRUE(diagram.startsWith("mindmap"));
}

TEST_F(MermaidMindmapDialogTest, DefaultDiagramContainsMeaningfulContent) {
    QString diagram = s_dlg->generatedDiagram();
    EXPECT_TRUE(diagram.contains("mindmap"));
    EXPECT_TRUE(diagram.contains("root"));
}
