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
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include "charts/AdvancedChartDialog.h"
#include "charts/ChartSource.h"

static int g_argc = 1;
static char g_arg0[] = "test_advanced_chart_dialog";
static char *g_argv[] = { g_arg0, nullptr };

class AdvancedChartDialogTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QCoreApplication::instance())
            new QApplication(g_argc, g_argv);
    }

    static QJsonObject specFromGenerated(const QString &generated) {
        int start = generated.indexOf(QStringLiteral("```ec\n")) + 5;
        int end = generated.indexOf(QStringLiteral("\n```"), start);
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(
            generated.mid(start, end - start).toUtf8(), &err);
        EXPECT_EQ(err.error, QJsonParseError::NoError);
        return doc.object();
    }

    // Changes the chart-type combo to select `label`, returning its index.
    int selectType(AdvancedChartDialog &dlg, const QString &label) {
        auto *combo = dlg.findChild<QComboBox *>();
        EXPECT_NE(combo, nullptr);
        const int idx = combo->findText(label);
        EXPECT_GE(idx, 0) << "no combo entry for " << label.toStdString();
        if (idx >= 0)
            combo->setCurrentIndex(idx);
        return idx;
    }

    QTableWidget *firstTable(AdvancedChartDialog &dlg) {
        auto *stack = dlg.findChild<QStackedWidget *>();
        if (!stack)
            return nullptr;
        auto *w = stack->currentWidget();
        if (auto *table = qobject_cast<QTableWidget *>(w))
            return table;
        return w ? w->findChild<QTableWidget *>() : nullptr;
    }

    AdvancedChartDialog dlg;
};

TEST_F(AdvancedChartDialogTest, DefaultIsSankeyAndWrappedInEcFence) {
    QString spec = dlg.generatedSpec();
    EXPECT_TRUE(spec.startsWith("\n```ec\n"));
    EXPECT_TRUE(spec.trimmed().endsWith("```"));
    QJsonObject obj = specFromGenerated(spec);
    EXPECT_EQ(obj["series"].toArray()[0].toObject()["type"].toString(), "sankey");
}

TEST_F(AdvancedChartDialogTest, DefaultSankeyHasThreeLinks) {
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray links = obj["series"].toArray()[0].toObject()["links"].toArray();
    ASSERT_EQ(links.size(), 3);
    EXPECT_EQ(links[0].toObject()["source"].toString(), "Coal");
    EXPECT_EQ(links[0].toObject()["target"].toString(), "Transport");
    EXPECT_EQ(links[0].toObject()["value"].toDouble(), 60);
}

TEST_F(AdvancedChartDialogTest, BoxplotSpecHasCategoryAxisAndFiveStats) {
    selectType(dlg, "Box Plot");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray cats = obj["xAxis"].toObject()["data"].toArray();
    ASSERT_EQ(cats.size(), 2);
    EXPECT_EQ(cats[0].toString(), "Class A");
    QJsonArray data = obj["series"].toArray()[0].toObject()["data"].toArray();
    ASSERT_EQ(data.size(), 2);
    ASSERT_EQ(data[0].toArray().size(), 5);
    EXPECT_EQ(data[0].toArray()[2].toDouble(), 71);
}

TEST_F(AdvancedChartDialogTest, ParallelSpecHasHeadersAsDimNames) {
    selectType(dlg, "Parallel Coordinates");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray axes = obj["parallelAxis"].toArray();
    ASSERT_EQ(axes.size(), 4);
    EXPECT_EQ(axes[0].toObject()["dim"].toInt(), 0);
    EXPECT_EQ(axes[0].toObject()["name"].toString(), "Dim 1");
    QJsonArray data = obj["series"].toArray()[0].toObject()["data"].toArray();
    ASSERT_EQ(data.size(), 3);
    ASSERT_EQ(data[0].toArray().size(), 4);
}

TEST_F(AdvancedChartDialogTest, ParallelAddColumnAddsDimension) {
    selectType(dlg, "Parallel Coordinates");
    auto *stack = dlg.findChild<QStackedWidget *>();
    ASSERT_NE(stack, nullptr);
    auto *table = qobject_cast<QTableWidget *>(stack->currentWidget());
    ASSERT_NE(table, nullptr);
    QPushButton *addCol = nullptr;
    for (QPushButton *b : dlg.findChildren<QPushButton *>())
        if (b->text() == QLatin1String("+Col"))
            addCol = b;
    ASSERT_NE(addCol, nullptr);
    addCol->click();
    EXPECT_EQ(table->columnCount(), 5);
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_EQ(obj["parallelAxis"].toArray().size(), 5);
}

TEST_F(AdvancedChartDialogTest, ThemeRiverSpecHasTriples) {
    selectType(dlg, "Theme River");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonArray data = obj["series"].toArray()[0].toObject()["data"].toArray();
    ASSERT_EQ(data.size(), 6);
    EXPECT_EQ(data[0].toArray()[0].toString(), "2026-06-01");
    EXPECT_EQ(data[0].toArray()[2].toString(), "Apple");
}

TEST_F(AdvancedChartDialogTest, TreemapInheritsTreeBySelection) {
    selectType(dlg, "Treemap");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonObject s = obj["series"].toArray()[0].toObject();
    EXPECT_EQ(s["type"].toString(), "treemap");
    QJsonArray data = s["data"].toArray();
    ASSERT_EQ(data.size(), 2);
    EXPECT_EQ(data[0].toObject()["name"].toString(), "nodeA");
    EXPECT_EQ(data[0].toObject()["children"].toArray().size(), 2);
}

TEST_F(AdvancedChartDialogTest, SunburstReusesSameTableEmitsSunburst) {
    selectType(dlg, "Sunburst");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_EQ(obj["series"].toArray()[0].toObject()["type"].toString(), "sunburst");
}

TEST_F(AdvancedChartDialogTest, GraphHasFourNodesAndFourLinks) {
    selectType(dlg, "Graph (Network)");
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    QJsonObject s = obj["series"].toArray()[0].toObject();
    ASSERT_EQ(s["data"].toArray().size(), 4);
    ASSERT_EQ(s["links"].toArray().size(), 4);
    EXPECT_EQ(s["data"].toArray()[0].toObject()["name"].toString(), "N1");
}

TEST_F(AdvancedChartDialogTest, GeneratesDetectableEcTypeForEveryType) {
    auto *combo = dlg.findChild<QComboBox *>();
    ASSERT_NE(combo, nullptr);
    const int types = combo->count();
    for (int i = 0; i < types; ++i) {
        combo->setCurrentIndex(i);
        const QJsonObject obj = specFromGenerated(dlg.generatedSpec());
        const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        EXPECT_NE(ChartSource::detectEcType(json), ChartSource::EcType::Unknown)
            << "combo item " << i << " produced an undetectable spec";
    }
}

TEST_F(AdvancedChartDialogTest, BlankRowsAreSkippedInSankeySpec) {
    // Clearing source+target on a row removes it from the emitted links.
    auto *table = firstTable(dlg);
    ASSERT_NE(table, nullptr);
    table->item(0, 0)->setText(QString());
    table->item(0, 1)->setText(QString());
    QJsonObject obj = specFromGenerated(dlg.generatedSpec());
    EXPECT_EQ(obj["series"].toArray()[0].toObject()["links"].toArray().size(), 2);
}