#include <gtest/gtest.h>
#include <QApplication>
#include <QComboBox>
#include "MermaidDialog.h"

static int g_argc = 1;
static char g_arg0[] = "test_mermaid_dialog";
static char *g_argv[] = {g_arg0, nullptr};

class MermaidDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    static QComboBox *chartTypeCombo(MermaidDialog &dlg) {
        const auto combos = dlg.findChildren<QComboBox*>();
        for (auto *c : combos)
            if (c->count() == 12) return c;
        return nullptr;
    }

    void selectChartType(MermaidDialog &dlg, int index) {
        auto *c = chartTypeCombo(dlg);
        ASSERT_NE(c, nullptr);
        c->setCurrentIndex(index);
    }
};

TEST_F(MermaidDialogTest, PieChart)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 0);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("```mermaid"));
    EXPECT_TRUE(block.contains("pie"));
    EXPECT_TRUE(block.contains("Alpha"));
    EXPECT_TRUE(block.contains("Beta"));
}

TEST_F(MermaidDialogTest, Flowchart)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 1);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("flowchart"));
    EXPECT_TRUE(block.contains("TD"));
    EXPECT_TRUE(block.contains("A[Start]"));
    EXPECT_TRUE(block.contains("B(Process)"));
    EXPECT_TRUE(block.contains("A-->B"));
}

TEST_F(MermaidDialogTest, SequenceDiagram)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 2);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("sequenceDiagram"));
    EXPECT_TRUE(block.contains("Alice"));
    EXPECT_TRUE(block.contains("Bob"));
    EXPECT_TRUE(block.contains("Hello"));
    EXPECT_TRUE(block.contains("Hi"));
}

TEST_F(MermaidDialogTest, GanttChart)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 3);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("gantt"));
    EXPECT_TRUE(block.contains("dateFormat"));
    EXPECT_TRUE(block.contains("Backend"));
    EXPECT_TRUE(block.contains("Frontend"));
    EXPECT_TRUE(block.contains("API design"));
    EXPECT_TRUE(block.contains("UI components"));
}

TEST_F(MermaidDialogTest, ClassDiagram)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 4);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("classDiagram"));
    EXPECT_TRUE(block.contains("Animal"));
    EXPECT_TRUE(block.contains("Dog"));
    EXPECT_TRUE(block.contains("<|--"));
    EXPECT_TRUE(block.contains("extends"));
}

TEST_F(MermaidDialogTest, ERDiagram)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 5);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("erDiagram"));
    EXPECT_TRUE(block.contains("USER"));
    EXPECT_TRUE(block.contains("POST"));
    EXPECT_TRUE(block.contains("||--o{"));
    EXPECT_TRUE(block.contains("writes"));
}

TEST_F(MermaidDialogTest, StateDiagram)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 6);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("stateDiagram-v2"));
    EXPECT_TRUE(block.contains("[*]"));
    EXPECT_TRUE(block.contains("-->"));
}

TEST_F(MermaidDialogTest, Mindmap)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 7);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("mindmap"));
    EXPECT_TRUE(block.contains("Central Idea"));
    EXPECT_TRUE(block.contains("Idea 1"));
    EXPECT_TRUE(block.contains("Idea 2"));
}

TEST_F(MermaidDialogTest, Timeline)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 8);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("timeline"));
    EXPECT_TRUE(block.contains("Q1 2026"));
    EXPECT_TRUE(block.contains("Q2 2026"));
    EXPECT_TRUE(block.contains("Launch v1.0"));
    EXPECT_TRUE(block.contains("Template library"));
}

TEST_F(MermaidDialogTest, Journey)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 9);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("journey"));
    EXPECT_TRUE(block.contains("Morning"));
    EXPECT_TRUE(block.contains("Work"));
    EXPECT_TRUE(block.contains("Wake up"));
    EXPECT_TRUE(block.contains("Coding"));
}

TEST_F(MermaidDialogTest, QuadrantChart)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 10);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("quadrantChart"));
    EXPECT_TRUE(block.contains("Reach"));
    EXPECT_TRUE(block.contains("Impact"));
    EXPECT_TRUE(block.contains("Quick Wins"));
    EXPECT_TRUE(block.contains("Feature A"));
    EXPECT_TRUE(block.contains("0.3"));
}

TEST_F(MermaidDialogTest, SankeyDiagram)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 11);
    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("sankey-beta"));
    EXPECT_TRUE(block.contains("Revenue"));
    EXPECT_TRUE(block.contains("COGS"));
    EXPECT_TRUE(block.contains("Product Sales"));
    EXPECT_TRUE(block.contains("600"));
}

TEST_F(MermaidDialogTest, AllChartTypesProduceOutput)
{
    for (int i = 0; i < 12; ++i) {
        MermaidDialog dlg{""};
        selectChartType(dlg, i);
        QString block = dlg.mermaidBlock();
        EXPECT_FALSE(block.isEmpty()) << "Chart type index " << i << " produced empty output";
        EXPECT_TRUE(block.contains("```mermaid")) << "Chart type index " << i << " missing mermaid fence";
    }
}
