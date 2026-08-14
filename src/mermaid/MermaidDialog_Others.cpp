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


#include "MermaidDialog.h"
#include "io/CsvReader.h"
#include "io/CsvColumnMapDialog.h"
#include "GitGraphBuilder.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

void MermaidDialog::populateComboColumns(QTableWidget *table,
                                          const QList<int> &columns,
                                          const QStringList &items)
{
    for (int r = 0; r < table->rowCount(); ++r) {
        for (int col : columns) {
            auto *box = qobject_cast<QComboBox*>(table->cellWidget(r, col));
            if (!box) {
                box = new QComboBox(table);
                table->setCellWidget(r, col, box);
                connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MermaidDialog::schedulePreviewUpdate);
            }
            QString cur = box->currentText();
            box->blockSignals(true);
            box->clear();
            box->addItems(items);
            int idx = box->findText(cur);
            if (idx >= 0)
                box->setCurrentIndex(idx);
            box->blockSignals(false);
        }
    }
}
void MermaidDialog::csvImportForChart(QTableWidget *table, const QStringList &chartFields,
                                       const QList<int> &columnIndices)
{
    CsvColumnMapDialog dlg(chartFields, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QHash<QString, int> mapping = dlg.mapping();
    const CsvData &data = dlg.csvData();
    if (data.rows.isEmpty())
        return;

    table->blockSignals(true);
    int nCols = qMin(columnIndices.size(), chartFields.size());
    int nRows = data.rows.size();
    table->setRowCount(nRows);

    for (int r = 0; r < nRows; ++r) {
        for (int c = 0; c < nCols; ++c) {
            int csvCol = mapping.value(chartFields[c], -1);
            int tableCol = columnIndices[c];
            if (tableCol < 0 || tableCol >= table->columnCount())
                continue;
            QString val;
            if (csvCol >= 0 && csvCol < data.rows[r].size())
                val = data.rows[r][csvCol];
            table->setItem(r, tableCol, new QTableWidgetItem(val));
        }
    }
    table->blockSignals(false);
    schedulePreviewUpdate();
}
QString MermaidDialog::buildPieDiagram() const
{
    QString out;
    QString title = m_pieTitle->text().trimmed();
    if (!title.isEmpty())
        out += "pie title " + title + "\n";
    else
        out += "pie\n";

    for (int r = 0; r < m_pieTable->rowCount(); ++r) {
        auto *labelItem = m_pieTable->item(r, 0);
        auto *valueItem = m_pieTable->item(r, 1);
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString value = valueItem ? valueItem->text().trimmed() : QString();
        if (!label.isEmpty() && !value.isEmpty())
            out += "    \"" + label + "\" : " + value + "\n";
    }
    return out;
}
QString MermaidDialog::buildMindmapDiagram() const
{
    QString out = "mindmap\n";
    QTreeWidgetItem *root = m_mindmapTree->topLevelItem(0);
    if (root) {
        std::function<QString(QTreeWidgetItem*, int)> buildNode;
        buildNode = [&buildNode](QTreeWidgetItem *item, int depth) -> QString {
            QString indent(depth * 4, ' ');
            QString text = item->text(0);
            QString out;
            if (depth == 1)
                out = indent + "root((" + text + "))\n";
            else
                out = indent + text + "\n";
            for (int i = 0; i < item->childCount(); ++i)
                out += buildNode(item->child(i), depth + 1);
            return out;
        };
        out += buildNode(root, 1);
    }
    return out;
}
QString MermaidDialog::buildTimelineDiagram() const
{
    QString title = m_timelineTitle->text().trimmed();
    QString out = "timeline\n";
    if (!title.isEmpty()) out += "    title " + title + "\n";
    QString currentSection;
    for (int r = 0; r < m_timelineTable->rowCount(); ++r) {
        QString section = m_timelineTable->item(r, 0) ? m_timelineTable->item(r, 0)->text().trimmed() : QString();
        QString event = m_timelineTable->item(r, 1) ? m_timelineTable->item(r, 1)->text().trimmed() : QString();
        if (section.isEmpty() && event.isEmpty()) continue;
        if (section != currentSection) {
            if (!section.isEmpty()) { out += "    " + section + "\n"; currentSection = section; }
        }
        if (!event.isEmpty()) out += "            : " + event + "\n";
    }
    return out;
}
QString MermaidDialog::buildJourneyDiagram() const
{
    QString title = m_journeyTitle->text().trimmed();
    QString out = "journey\n";
    if (!title.isEmpty()) out += "    title " + title + "\n";
    QString currentSection;
    for (int r = 0; r < m_journeyTable->rowCount(); ++r) {
        QString section = m_journeyTable->item(r, 0) ? m_journeyTable->item(r, 0)->text().trimmed() : QString();
        QString task = m_journeyTable->item(r, 1) ? m_journeyTable->item(r, 1)->text().trimmed() : QString();
        auto *spin = qobject_cast<QSpinBox*>(m_journeyTable->cellWidget(r, 2));
        int score = spin ? spin->value() : 5;
        QString actors = m_journeyTable->item(r, 3) ? m_journeyTable->item(r, 3)->text().trimmed() : QString();
        if (section.isEmpty() && task.isEmpty()) continue;
        if (section != currentSection && !section.isEmpty()) { out += "    section " + section + "\n"; currentSection = section; }
        if (!task.isEmpty()) out += "        " + task + ": " + QString::number(score) + ": " + actors + "\n";
    }
    return out;
}
QString MermaidDialog::buildQuadrantDiagram() const
{
    QString out = "quadrantChart\n";
    QString title = m_quadTitle->text().trimmed();
    if (!title.isEmpty()) out += "    title " + title + "\n";
    out += "    x-axis " + m_quadXLeft->text().trimmed() + " --> " + m_quadXRight->text().trimmed() + "\n";
    out += "    y-axis " + m_quadYBottom->text().trimmed() + " --> " + m_quadYTop->text().trimmed() + "\n";
    if (!m_quadQ1->text().trimmed().isEmpty()) out += "    quadrant-1 " + m_quadQ1->text().trimmed() + "\n";
    if (!m_quadQ2->text().trimmed().isEmpty()) out += "    quadrant-2 " + m_quadQ2->text().trimmed() + "\n";
    if (!m_quadQ3->text().trimmed().isEmpty()) out += "    quadrant-3 " + m_quadQ3->text().trimmed() + "\n";
    if (!m_quadQ4->text().trimmed().isEmpty()) out += "    quadrant-4 " + m_quadQ4->text().trimmed() + "\n";
    for (int r = 0; r < m_quadTable->rowCount(); ++r) {
        QString label = m_quadTable->item(r, 0) ? m_quadTable->item(r, 0)->text().trimmed() : QString();
        QString x = m_quadTable->item(r, 1) ? m_quadTable->item(r, 1)->text().trimmed() : QString();
        QString y = m_quadTable->item(r, 2) ? m_quadTable->item(r, 2)->text().trimmed() : QString();
        if (!label.isEmpty() && !x.isEmpty() && !y.isEmpty())
            out += "    " + label + ": [" + x + ", " + y + "]\n";
    }
    return out;
}
QString MermaidDialog::buildSankeyDiagram() const
{
    QString out = "sankey-beta\n";
    for (int r = 0; r < m_sankeyTable->rowCount(); ++r) {
        QString src = m_sankeyTable->item(r, 0) ? m_sankeyTable->item(r, 0)->text().trimmed() : QString();
        QString tgt = m_sankeyTable->item(r, 1) ? m_sankeyTable->item(r, 1)->text().trimmed() : QString();
        QString val = m_sankeyTable->item(r, 2) ? m_sankeyTable->item(r, 2)->text().trimmed() : QString();
        if (!src.isEmpty() && !tgt.isEmpty() && !val.isEmpty())
            out += "    " + src + "," + tgt + "," + val + "\n";
    }
    return out;
}

QWidget *MermaidDialog::createRadarPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Title:")));
    m_radarTitle = new QLineEdit(panel);
    m_radarTitle->setPlaceholderText("My Radar");
    m_radarTitle->setText("My Radar");
    layout->addWidget(m_radarTitle);

    layout->addWidget(new QLabel(tr("Axes (ID, Label):")));
    auto *axisBtnLayout = new QHBoxLayout();
    auto *addAxisBtn = new QPushButton("+Row", panel);
    axisBtnLayout->addWidget(addAxisBtn);
    axisBtnLayout->addStretch();
    layout->addLayout(axisBtnLayout);

    const int axisDelCol = 2;
    m_radarAxisTable = new QTableWidget(3, 3, panel);
    m_radarAxisTable->setHorizontalHeaderLabels({"ID", "Label", "Del"});
    m_radarAxisTable->setItem(0, 0, new QTableWidgetItem("speed"));
    m_radarAxisTable->setItem(0, 1, new QTableWidgetItem("Speed"));
    m_radarAxisTable->setItem(1, 0, new QTableWidgetItem("reliability"));
    m_radarAxisTable->setItem(1, 1, new QTableWidgetItem("Reliability"));
    m_radarAxisTable->setItem(2, 0, new QTableWidgetItem("comfort"));
    m_radarAxisTable->setItem(2, 1, new QTableWidgetItem("Comfort"));
    m_radarAxisTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_radarAxisTable->horizontalHeader()->setSectionResizeMode(axisDelCol, QHeaderView::Fixed);
    m_radarAxisTable->setColumnWidth(axisDelCol, 32);
    m_radarAxisTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_radarAxisTable->rowCount(); ++r)
        addDeleteButton(m_radarAxisTable, axisDelCol, r);
    layout->addWidget(m_radarAxisTable);

    layout->addWidget(new QLabel(tr("Curves (ID, Label, Values):")));
    auto *curveBtnLayout = new QHBoxLayout();
    auto *addCurveBtn = new QPushButton("+Row", panel);
    curveBtnLayout->addWidget(addCurveBtn);
    curveBtnLayout->addStretch();
    layout->addLayout(curveBtnLayout);

    const int curveDelCol = 3;
    m_radarCurveTable = new QTableWidget(2, 4, panel);
    m_radarCurveTable->setHorizontalHeaderLabels({"ID", "Label", "Values", "Del"});
    m_radarCurveTable->setItem(0, 0, new QTableWidgetItem("car"));
    m_radarCurveTable->setItem(0, 1, new QTableWidgetItem("Car"));
    m_radarCurveTable->setItem(0, 2, new QTableWidgetItem("6, 9, 8"));
    m_radarCurveTable->setItem(1, 0, new QTableWidgetItem("bike"));
    m_radarCurveTable->setItem(1, 1, new QTableWidgetItem("Bike"));
    m_radarCurveTable->setItem(1, 2, new QTableWidgetItem("4, 3, 6"));
    m_radarCurveTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_radarCurveTable->horizontalHeader()->setSectionResizeMode(curveDelCol, QHeaderView::Fixed);
    m_radarCurveTable->setColumnWidth(curveDelCol, 32);
    m_radarCurveTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_radarCurveTable->rowCount(); ++r)
        addDeleteButton(m_radarCurveTable, curveDelCol, r);
    layout->addWidget(m_radarCurveTable);

    auto *optionsLayout = new QHBoxLayout();
    m_radarShowLegend = new QCheckBox(tr("Show legend"), panel);
    m_radarShowLegend->setChecked(true);
    optionsLayout->addWidget(m_radarShowLegend);
    optionsLayout->addWidget(new QLabel(tr("Min:")));
    m_radarMin = new QSpinBox(panel);
    m_radarMin->setRange(0, 100000);
    m_radarMin->setValue(0);
    optionsLayout->addWidget(m_radarMin);
    optionsLayout->addWidget(new QLabel(tr("Max:")));
    m_radarMax = new QSpinBox(panel);
    m_radarMax->setRange(0, 1000000);
    m_radarMax->setValue(100);
    optionsLayout->addWidget(m_radarMax);
    optionsLayout->addWidget(new QLabel(tr("Graticule:")));
    m_radarGraticule = new QComboBox(panel);
    m_radarGraticule->addItem("Circle", "circle");
    m_radarGraticule->addItem("Polygon", "polygon");
    optionsLayout->addWidget(m_radarGraticule);
    optionsLayout->addWidget(new QLabel(tr("Ticks:")));
    m_radarTicks = new QSpinBox(panel);
    m_radarTicks->setRange(1, 20);
    m_radarTicks->setValue(5);
    optionsLayout->addWidget(m_radarTicks);
    optionsLayout->addStretch();
    layout->addLayout(optionsLayout);

    layout->addStretch();

    connect(m_radarTitle, &QLineEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarAxisTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarCurveTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarShowLegend, &QCheckBox::toggled, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarMin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarMax, QOverload<int>::of(&QSpinBox::valueChanged), this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarGraticule, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_radarTicks, QOverload<int>::of(&QSpinBox::valueChanged), this, &MermaidDialog::schedulePreviewUpdate);
    connect(addAxisBtn, &QPushButton::clicked, this, [this]() {
        int row = m_radarAxisTable->rowCount();
        m_radarAxisTable->insertRow(row);
        addDeleteButton(m_radarAxisTable, axisDelCol, row);
        schedulePreviewUpdate();
    });
    connect(addCurveBtn, &QPushButton::clicked, this, [this]() {
        int row = m_radarCurveTable->rowCount();
        m_radarCurveTable->insertRow(row);
        addDeleteButton(m_radarCurveTable, curveDelCol, row);
        schedulePreviewUpdate();
    });

    return panel;
}

QString MermaidDialog::buildRadarDiagram() const
{
    QString out = "radar-beta\n";
    QString title = m_radarTitle->text().trimmed();
    if (!title.isEmpty())
        out += "  title " + title + "\n";

    QStringList axisParts;
    for (int r = 0; r < m_radarAxisTable->rowCount(); ++r) {
        QString id = m_radarAxisTable->item(r, 0) ? m_radarAxisTable->item(r, 0)->text().trimmed() : QString();
        QString label = m_radarAxisTable->item(r, 1) ? m_radarAxisTable->item(r, 1)->text().trimmed() : QString();
        if (!id.isEmpty() && !label.isEmpty())
            axisParts += id + "[\"" + label + "\"]";
    }
    if (!axisParts.isEmpty())
        out += "  axis " + axisParts.join(", ") + "\n";

    for (int r = 0; r < m_radarCurveTable->rowCount(); ++r) {
        QString id = m_radarCurveTable->item(r, 0) ? m_radarCurveTable->item(r, 0)->text().trimmed() : QString();
        QString label = m_radarCurveTable->item(r, 1) ? m_radarCurveTable->item(r, 1)->text().trimmed() : QString();
        QString values = m_radarCurveTable->item(r, 2) ? m_radarCurveTable->item(r, 2)->text().trimmed() : QString();
        if (!id.isEmpty() && !label.isEmpty() && !values.isEmpty())
            out += "  curve " + id + "[\"" + label + "\"]{" + values + "}\n";
    }

    out += "  showLegend " + QString(m_radarShowLegend->isChecked() ? "true" : "false") + "\n";
    out += "  max " + QString::number(m_radarMax->value()) + "\n";
    out += "  min " + QString::number(m_radarMin->value()) + "\n";
    out += "  graticule " + m_radarGraticule->currentData().toString() + "\n";
    out += "  ticks " + QString::number(m_radarTicks->value()) + "\n";

    return out;
}
QString MermaidDialog::buildGitGraphDiagram() const
{
    if (m_gitRepo.isEmpty())
        return {};
    GitGraphBuilder::Options opts;
    opts.limit = static_cast<GitGraphBuilder::Options::Limit>(
        m_gitLimitCombo->currentData().toInt());
    opts.branch = m_gitBranchCombo->currentText();
    opts.from = m_gitFromDate->date().startOfDay();
    opts.to = m_gitToDate->date().endOfDay();
    opts.maxCommits = m_gitNoLimit->isChecked() ? 0 : m_gitMaxCommits->value();
    QString error;
    return GitGraphBuilder::build(m_gitRepo, opts, &error);
}
