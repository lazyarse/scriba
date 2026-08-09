// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include <gtest/gtest.h>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QTest>
#include <QTemporaryDir>
#include "GitTestRepo.h"
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
            if (c->count() == 14) return c;
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

TEST_F(MermaidDialogTest, StateDiagramCompositeSectionRoundTrip)
{
    // A state diagram whose inner flow lives in a composite `state X { }`
    // block must round-trip: the section survives prefill and is re-emitted as
    // a nested block (not flattened into top-level transitions).
    const QString diagram =
        "stateDiagram-v2\n"
        "  [*] --> Idle\n"
        "  Idle --> Processing : start\n"
        "  state Processing {\n"
        "    [*] --> FetchData\n"
        "    FetchData --> Validate\n"
        "    Validate --> [*]\n"
        "  }\n"
        "  Processing --> Done\n";
    MermaidDialog dlg{diagram, QString()};
    QString block = dlg.mermaidBlock();
    EXPECT_TRUE(block.contains("stateDiagram-v2"));
    EXPECT_TRUE(block.contains("state Processing {"));
    EXPECT_TRUE(block.contains("        [*] --> FetchData"));
    EXPECT_TRUE(block.contains("        FetchData --> Validate"));
    EXPECT_TRUE(block.contains("        Validate --> [*]"));
    EXPECT_TRUE(block.contains("    }"));
    EXPECT_TRUE(block.contains("Idle --> Processing : start"));
    EXPECT_TRUE(block.contains("Processing --> Done"));
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

TEST_F(MermaidDialogTest, GitGraphPanelLoadsRepository)
{
    QTemporaryDir repo;
    ASSERT_TRUE(repo.isValid());
    ASSERT_TRUE(GitTestRepo::create(repo.path()));

    MermaidDialog dlg{""};
    selectChartType(dlg, 12);

    const auto edits = dlg.findChildren<QLineEdit*>();
    QLineEdit *pathEdit = nullptr;
    for (auto *e : edits) {
        if (e->placeholderText().contains("git repository")) {
            pathEdit = e;
            break;
        }
    }
    ASSERT_NE(pathEdit, nullptr);
    pathEdit->setText(repo.path());
    QTest::keyClick(pathEdit, Qt::Key_Return);

    QString block = dlg.mermaidBlock();
    EXPECT_FALSE(block.isEmpty());
    EXPECT_TRUE(block.contains("```mermaid"));
    EXPECT_TRUE(block.contains("gitGraph"));
    EXPECT_TRUE(block.contains("commit id:"));
    EXPECT_TRUE(block.contains("merge feature"));
}

TEST_F(MermaidDialogTest, GitGraphPanelWithoutRepoProducesNothing)
{
    MermaidDialog dlg{""};
    selectChartType(dlg, 12);
    EXPECT_TRUE(dlg.mermaidBlock().isEmpty());
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
