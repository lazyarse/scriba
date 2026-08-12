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


// ============================================================
//  Flowchart
// ============================================================
// ============================================================
//  Sequence Diagram
// ============================================================
// ============================================================
//  Gantt Chart
// ============================================================
// ============================================================
//  Class Diagram
// ============================================================
// ============================================================
//  ER Diagram
// ============================================================
// ============================================================
//  State Diagram
// ============================================================
// ============================================================
//  Mindmap
// ============================================================
// ============================================================
//  Timeline
// ============================================================
// ============================================================
//  User Journey
// ============================================================
// ============================================================
//  Quadrant Chart
// ============================================================
// ============================================================
//  Sankey Diagram
// ============================================================
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
#include "Preview.h"
#include "StaticHelpers.h"
#include "CssUtils.h"
#include "CsvReader.h"
#include "CsvColumnMapDialog.h"
#include "ChartSource.h"
#include "GitGraphBuilder.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <functional>

namespace {

constexpr int kSourcePanelIndex = 13; // index of the raw-source fallback panel

}

struct ArrowInfo {
    QString display;
    QString left;
    QString right;
};

static const ArrowInfo kArrowTypes[] = {
    {"-->",  "--",  "-->"},
    {"---",  "--",  "---"},
    {"-.->", "-.",  ".->"},
    {"==>",  "==",  "==>"},
    {"--o",  "--",  "--o"},
    {"--x",  "--",  "--x"},
    {"<-->", "<--", "-->"},
    {"<==>", "<==", "==>"},
    {"<-.->", "<-.", ".->"},
    {"o--o", "o--", "--o"},
    {"x--x", "x--", "--x"},
};

static const int kArrowCount = sizeof(kArrowTypes) / sizeof(ArrowInfo);

MermaidDialog::MermaidDialog(const QString &themeCss, QWidget *parent)
    : QDialog(parent)
    , m_mermaidTheme(CssUtils::isDarkTheme(themeCss) ? QStringLiteral("dark")
                                                     : QStringLiteral("default"))
{
    auto colors = CssUtils::themeColors(themeCss);
    m_bgColor = colors.background.name();
    m_iconColor = colors.text;
    m_themeCss = themeCss;

    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

MermaidDialog::MermaidDialog(const QString &existingDiagram, const QString &themeCss, QWidget *parent)
    : QDialog(parent)
    , m_mermaidTheme(CssUtils::isDarkTheme(themeCss) ? QStringLiteral("dark")
                                                     : QStringLiteral("default"))
{
    auto colors = CssUtils::themeColors(themeCss);
    m_bgColor = colors.background.name();
    m_iconColor = colors.text;
    m_themeCss = themeCss;

    setupUi();
    prefillFromSource(existingDiagram);
    updatePreview();
    schedulePreviewUpdate();
}

QString MermaidDialog::mermaidPreviewHtml(const QString &escaped, const QString &theme,
                                           const QString &bgColor)
{
    return QString(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>body{margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:sans-serif;background-color:%3;}"
        ".error{color:#d32f2f;padding:16px;}"
        ".mermaid{width:100%;}"
        ".mermaid svg[aria-roledescription=\"gantt\"]{width:100% !important;max-width:100% !important;height:auto;}</style>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "</head><body><div class=\"mermaid\">%1</div>"
        "<script>mermaid.initialize({startOnLoad:false,theme:'%2'});"
        "try{mermaid.run({querySelector:'.mermaid'}).catch(function(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';});"
        "}catch(e){document.body.innerHTML='<div class=\"error\">'+e+'</div>';}</script></body></html>"
    ).arg(escaped, theme, bgColor);
}

QString MermaidDialog::emptyPreviewHtml(const QString &bgColor)
{
    return QString(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>body{margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:sans-serif;background-color:%1;}"
        ".hint{color:palette(mid);}</style>"
        "</head><body><span class=\"hint\"></span></body></html>"
    ).arg(bgColor);
}

QString MermaidDialog::mermaidBlock() const
{
    QString diagram = buildDiagram();
    if (diagram.isEmpty())
        return {};
    if (sourceMode())
        return QStringLiteral("\n```mermaid\n%1\n```\n").arg(diagram);
    int w = m_widthCheck && m_widthCheck->isChecked() && m_widthSpin ? m_widthSpin->value() : 0;
    if (w > 0)
        return QStringLiteral("\n<div style=\"max-width:%1px\">\n\n```mermaid\n%2\n```\n\n</div>\n")
            .arg(w)
            .arg(diagram);
    return QStringLiteral("\n```mermaid\n%1\n```\n").arg(diagram);
}

void MermaidDialog::setupUi()
{
    setWindowTitle(tr("Mermaid Diagrams"));

    auto *mainLayout = new QVBoxLayout(this);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidDialog::updatePreview);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel
    auto *leftPanel = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Chart type selector
    m_chartTypeCombo = new QComboBox(leftPanel);
    m_chartTypeCombo->addItem(tr("Pie Chart"),             static_cast<int>(ChartType::Pie));
    m_chartTypeCombo->addItem(tr("Flowchart"),             static_cast<int>(ChartType::Flowchart));
    m_chartTypeCombo->addItem(tr("Sequence Diagram"),      static_cast<int>(ChartType::Sequence));
    m_chartTypeCombo->addItem(tr("Gantt Chart"),           static_cast<int>(ChartType::Gantt));
    m_chartTypeCombo->addItem(tr("Class Diagram"),         static_cast<int>(ChartType::Class));
    m_chartTypeCombo->addItem(tr("ER Diagram"),            static_cast<int>(ChartType::ER));
    m_chartTypeCombo->addItem(tr("State Diagram"),         static_cast<int>(ChartType::State));
    m_chartTypeCombo->addItem(tr("Mind Map"),              static_cast<int>(ChartType::Mindmap));
    m_chartTypeCombo->addItem(tr("Timeline"),              static_cast<int>(ChartType::Timeline));
    m_chartTypeCombo->addItem(tr("User Journey"),          static_cast<int>(ChartType::Journey));
    m_chartTypeCombo->addItem(tr("Quadrant Chart"),        static_cast<int>(ChartType::Quadrant));
    m_chartTypeCombo->addItem(tr("Sankey Diagram"),        static_cast<int>(ChartType::Sankey));
    m_chartTypeCombo->addItem(tr("Git Graph"),             static_cast<int>(ChartType::GitGraph));
    m_chartTypeCombo->addItem(tr("Diagram Source"),        kSourcePanelIndex);
    leftLayout->addWidget(m_chartTypeCombo);

    // Stacked widget for chart panels
    m_panels = new QStackedWidget(leftPanel);
    m_panels->addWidget(createPiePanel());       // 0
    m_panels->addWidget(createFlowchartPanel()); // 1
    m_panels->addWidget(createSequencePanel());  // 2
    m_panels->addWidget(createGanttPanel());     // 3
    m_panels->addWidget(createClassPanel());     // 4
    m_panels->addWidget(createERPanel());        // 5
    m_panels->addWidget(createStatePanel());     // 6
    m_panels->addWidget(createMindmapPanel());   // 7
    m_panels->addWidget(createTimelinePanel());  // 8
    m_panels->addWidget(createJourneyPanel());   // 9
    m_panels->addWidget(createQuadrantPanel());  // 10
    m_panels->addWidget(createSankeyPanel());    // 11

    // 12 — git graph panel
    m_panels->addWidget(createGitGraphPanel());

    // 13 — raw-source fallback panel
    auto *sourcePanel = new QWidget(this);
    auto *sourceLayout = new QVBoxLayout(sourcePanel);
    sourceLayout->setContentsMargins(12, 12, 12, 12);
    auto *sourceNote = new QLabel(
        tr("This diagram could not be read back into the form, so it is shown "
           "as source. Edit the Mermaid source directly; it is inserted "
           "unchanged."), sourcePanel);
    sourceNote->setWordWrap(true);
    sourceLayout->addWidget(sourceNote);
    m_sourceEdit = new QPlainTextEdit(sourcePanel);
    m_sourceEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_sourceEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    connect(m_sourceEdit, &QPlainTextEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    sourceLayout->addWidget(m_sourceEdit, 1);
    m_panels->addWidget(sourcePanel);

    leftLayout->addWidget(m_panels, 1);

    // Right panel (preview)
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *widthRow = new QHBoxLayout;
    m_widthCheck = new QCheckBox(tr("Set max width:"), rightWidget);
    m_widthSpin = new QSpinBox(rightWidget);
    m_widthSpin->setRange(100, 2000);
    m_widthSpin->setValue(500);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setEnabled(false);
    connect(m_widthCheck, &QCheckBox::toggled, m_widthSpin, &QWidget::setEnabled);
    widthRow->addWidget(m_widthCheck);
    widthRow->addWidget(m_widthSpin);
    widthRow->addStretch();
    rightLayout->addLayout(widthRow);

    m_preview = createPreviewView(rightWidget, m_themeCss);
    rightLayout->addWidget(m_preview, 1);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 500});

    mainLayout->addWidget(splitter);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    auto *copyBtn = m_buttonBox->addButton(tr("&Copy"), QDialogButtonBox::ActionRole);
    auto *insertBtn = m_buttonBox->addButton(tr("&Insert"), QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    stripButtonIcons(m_buttonBox);
    mainLayout->addWidget(m_buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(buildDiagram());
    });
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::onChartTypeChanged);

    resize(950, 550);
}

void MermaidDialog::onChartTypeChanged(int index)
{
    m_panels->setCurrentIndex(index);
    schedulePreviewUpdate();
}

void MermaidDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidDialog::updatePreview()
{
    QString diagram = buildDiagram();
    if (diagram.trimmed().isEmpty()) {
        m_preview->setHtml(emptyPreviewHtml(m_bgColor));
        return;
    }
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped, m_mermaidTheme, m_bgColor));
}

void MermaidDialog::addDeleteButton(QTableWidget *table, int column, int row,
                                     std::function<void()> onDelete)
{
    auto *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", m_iconColor, 16), "", table);
    delBtn->setFixedSize(26, 22);
    delBtn->setToolTip("Delete row");
    delBtn->setFlat(true);
    delBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; padding: 0; }"
        " QPushButton:hover { background: transparent; }"
        " QPushButton:pressed { background: transparent; }");
    table->setCellWidget(row, column, delBtn);
    connect(delBtn, &QPushButton::clicked, this, [this, table, delBtn, onDelete = std::move(onDelete)]() {
        int row = table->indexAt(delBtn->pos()).row();
        if (row >= 0 && table->rowCount() > 1) {
            table->removeRow(row);
            if (onDelete)
                onDelete();
        }
        schedulePreviewUpdate();
    });
}
QString MermaidDialog::buildDiagram() const
{
    if (sourceMode())
        return m_sourceEdit ? m_sourceEdit->toPlainText() : QString();
    switch (static_cast<ChartType>(m_chartTypeCombo->currentData().toInt())) {
    case ChartType::Pie:        return buildPieDiagram();
    case ChartType::Flowchart:  return buildFlowchartDiagram();
    case ChartType::Sequence:   return buildSequenceDiagram();
    case ChartType::Gantt:      return buildGanttDiagram();
    case ChartType::Class:      return buildClassDiagram();
    case ChartType::ER:         return buildERDiagram();
    case ChartType::State:      return buildStateDiagram();
    case ChartType::Mindmap:    return buildMindmapDiagram();
    case ChartType::Timeline:   return buildTimelineDiagram();
    case ChartType::Journey:    return buildJourneyDiagram();
    case ChartType::Quadrant:   return buildQuadrantDiagram();
    case ChartType::Sankey:     return buildSankeyDiagram();
    case ChartType::GitGraph:   return buildGitGraphDiagram();
    }
    return {};
}

bool MermaidDialog::sourceMode() const
{
    return m_chartTypeCombo && m_chartTypeCombo->currentData().toInt() == kSourcePanelIndex;
}

void MermaidDialog::setChartType(int index)
{
    int i = m_chartTypeCombo->findData(index);
    if (i >= 0)
        m_chartTypeCombo->setCurrentIndex(i);
}

void MermaidDialog::prefillFromSource(const QString &diagram)
{
    ChartSource::MermaidData data;
    if (!ChartSource::parseMermaid(diagram, data)) {
        if (m_sourceEdit)
            m_sourceEdit->setPlainText(diagram);
        setChartType(kSourcePanelIndex);
        return;
    }
    applyPrefill(data);
}

void MermaidDialog::applyPrefill(const ChartSource::MermaidData &d)
{
    switch (d.type) {
    case ChartSource::MermaidType::Pie:
        if (m_pieTitle) m_pieTitle->setText(d.pieTitle);
        m_pieTable->setRowCount(d.pieEntries.size());
        for (int r = 0; r < d.pieEntries.size(); ++r) {
            m_pieTable->setItem(r, 0, new QTableWidgetItem(d.pieEntries[r].first));
            m_pieTable->setItem(r, 1, new QTableWidgetItem(QString::number(d.pieEntries[r].second)));
        }
        setChartType(static_cast<int>(ChartType::Pie));
        break;

    case ChartSource::MermaidType::Flowchart: {
        int di = m_fcDirection->findData(d.fcDirection);
        if (di >= 0) m_fcDirection->setCurrentIndex(di);
        m_fcNodeTable->setRowCount(d.fcNodes.size());
        for (int r = 0; r < d.fcNodes.size(); ++r) {
            m_fcNodeTable->setItem(r, 0, new QTableWidgetItem(d.fcNodes[r][0]));
            m_fcNodeTable->setItem(r, 1, new QTableWidgetItem(d.fcNodes[r][1]));
            auto *shape = qobject_cast<QComboBox*>(m_fcNodeTable->cellWidget(r, 2));
            if (!shape) {
                shape = new QComboBox(m_fcNodeTable);
                shape->addItems({"Box", "Round", "Stadium", "Diamond", "Hexagon"});
                m_fcNodeTable->setCellWidget(r, 2, shape);
                connect(shape, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MermaidDialog::schedulePreviewUpdate);
            }
            int si = shape->findText(d.fcNodes[r][2]);
            if (si >= 0) shape->setCurrentIndex(si);
        }
        m_fcEdgeTable->setRowCount(d.fcEdges.size());
        for (int r = 0; r < d.fcEdges.size(); ++r)
            m_fcEdgeTable->setItem(r, 2, new QTableWidgetItem(d.fcEdges[r][2]));
        refreshEdgeNodeCombos();
        for (int r = 0; r < d.fcEdges.size(); ++r) {
            if (auto *from = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 0)))
                from->setCurrentText(d.fcEdges[r][0]);
            if (auto *to = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 1)))
                to->setCurrentText(d.fcEdges[r][1]);
            if (auto *arrow = qobject_cast<QComboBox*>(m_fcEdgeTable->cellWidget(r, 3)))
                arrow->setCurrentText(d.fcEdges[r][3]);
        }
        setChartType(static_cast<int>(ChartType::Flowchart));
        break;
    }

    case ChartSource::MermaidType::Sequence:
        m_seqParticipantTable->setRowCount(d.seqParticipants.size());
        for (int r = 0; r < d.seqParticipants.size(); ++r) {
            m_seqParticipantTable->setItem(r, 0, new QTableWidgetItem(d.seqParticipants[r][0]));
            m_seqParticipantTable->setItem(r, 1, new QTableWidgetItem(d.seqParticipants[r][1]));
        }
        m_seqMessageTable->setRowCount(d.seqMessages.size());
        for (int r = 0; r < d.seqMessages.size(); ++r)
            m_seqMessageTable->setItem(r, 2, new QTableWidgetItem(d.seqMessages[r][2]));
        refreshMessageCombos();
        for (int r = 0; r < d.seqMessages.size(); ++r) {
            if (auto *from = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 0)))
                from->setCurrentText(d.seqMessages[r][0]);
            if (auto *to = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 1)))
                to->setCurrentText(d.seqMessages[r][1]);
            if (auto *arrow = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(r, 3)))
                arrow->setCurrentText(d.seqMessages[r][3]);
        }
        setChartType(static_cast<int>(ChartType::Sequence));
        break;

    case ChartSource::MermaidType::Gantt:
        if (m_ganttTitle) m_ganttTitle->setText(d.ganttTitle);
        if (m_ganttWeekend) m_ganttWeekend->setChecked(d.ganttWeekend);
        {
            int di = m_ganttDateFormat->findText(d.ganttDateFormat);
            if (di >= 0) m_ganttDateFormat->setCurrentIndex(di);
        }
        m_ganttTaskTable->setRowCount(d.ganttTasks.size());
        for (int r = 0; r < d.ganttTasks.size(); ++r) {
            const QStringList &t = d.ganttTasks[r];
            m_ganttTaskTable->setItem(r, 0, new QTableWidgetItem(t[0]));
            m_ganttTaskTable->setItem(r, 1, new QTableWidgetItem(t[1]));
            m_ganttTaskTable->setItem(r, 2, new QTableWidgetItem(t[2]));
            m_ganttTaskTable->setItem(r, 3, new QTableWidgetItem(t[3]));
            m_ganttTaskTable->setItem(r, 5, new QTableWidgetItem(t[5]));
            auto *status = new QComboBox(m_ganttTaskTable);
            status->addItem("", "");
            status->addItem("Done", "done");
            status->addItem("Active", "active");
            status->addItem("Crit", "crit");
            status->addItem("Milestone", "milestone");
            int si = status->findData(t[4]);
            if (si >= 0) status->setCurrentIndex(si);
            m_ganttTaskTable->setCellWidget(r, 4, status);
            connect(status, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialog::schedulePreviewUpdate);
        }
        setChartType(static_cast<int>(ChartType::Gantt));
        break;

    case ChartSource::MermaidType::State: {
        QStringList states;
        for (const QStringList &t : d.stateTransitions) {
            for (const QString &s : {t[0], t[1]})
                if (!states.contains(s))
                    states.append(s);
        }
        m_stateTable->setRowCount(states.size());
        for (int r = 0; r < states.size(); ++r) {
            m_stateTable->setItem(r, 0, new QTableWidgetItem(states[r]));
            m_stateTable->setItem(r, 1, new QTableWidgetItem(QString()));
        }
        m_stateTransitionTable->setRowCount(d.stateTransitions.size());
        for (int r = 0; r < d.stateTransitions.size(); ++r) {
            m_stateTransitionTable->setItem(r, 2, new QTableWidgetItem(d.stateTransitions[r][2]));
            m_stateTransitionTable->setItem(r, 3,
                new QTableWidgetItem(d.stateTransitions[r].size() > 3 ? d.stateTransitions[r][3] : QString()));
        }
        refreshStateCombos();
        for (int r = 0; r < d.stateTransitions.size(); ++r) {
            if (auto *from = qobject_cast<QComboBox*>(m_stateTransitionTable->cellWidget(r, 0)))
                from->setCurrentText(d.stateTransitions[r][0]);
            if (auto *to = qobject_cast<QComboBox*>(m_stateTransitionTable->cellWidget(r, 1)))
                to->setCurrentText(d.stateTransitions[r][1]);
        }
        setChartType(static_cast<int>(ChartType::State));
        break;
    }

    case ChartSource::MermaidType::Mindmap: {
        m_mindmapTree->clear();
        for (const ChartSource::MermaidData::TreeNode &root : d.mindmapRoots) {
            std::function<QTreeWidgetItem*(const ChartSource::MermaidData::TreeNode&, QTreeWidgetItem*)> addNode;
            addNode = [&](const ChartSource::MermaidData::TreeNode &node, QTreeWidgetItem *parent) {
                auto *item = new QTreeWidgetItem(
                    parent ? parent : m_mindmapTree->invisibleRootItem());
                item->setText(0, node.text);
                item->setExpanded(true);
                for (const auto &child : node.children)
                    addNode(child, item);
                return item;
            };
            addNode(root, nullptr);
        }
        setChartType(static_cast<int>(ChartType::Mindmap));
        break;
    }

    case ChartSource::MermaidType::Timeline:
        if (m_timelineTitle) m_timelineTitle->setText(d.timelineTitle);
        m_timelineTable->setRowCount(d.timelineEntries.size());
        for (int r = 0; r < d.timelineEntries.size(); ++r) {
            m_timelineTable->setItem(r, 0, new QTableWidgetItem(d.timelineEntries[r][0]));
            m_timelineTable->setItem(r, 1, new QTableWidgetItem(d.timelineEntries[r][1]));
        }
        setChartType(static_cast<int>(ChartType::Timeline));
        break;

    case ChartSource::MermaidType::Journey:
        if (m_journeyTitle) m_journeyTitle->setText(d.journeyTitle);
        m_journeyTable->setRowCount(d.journeyEntries.size());
        for (int r = 0; r < d.journeyEntries.size(); ++r) {
            m_journeyTable->setItem(r, 0, new QTableWidgetItem(d.journeyEntries[r][0]));
            m_journeyTable->setItem(r, 1, new QTableWidgetItem(d.journeyEntries[r][1]));
            auto *spin = new QSpinBox(m_journeyTable);
            spin->setRange(1, 7);
            spin->setValue(d.journeyEntries[r][2].toInt());
            m_journeyTable->setCellWidget(r, 2, spin);
            m_journeyTable->setItem(r, 3, new QTableWidgetItem(d.journeyEntries[r][3]));
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &MermaidDialog::schedulePreviewUpdate);
        }
        setChartType(static_cast<int>(ChartType::Journey));
        break;

    case ChartSource::MermaidType::Quadrant:
        if (m_quadTitle) m_quadTitle->setText(d.quadTitle);
        if (m_quadXLeft) m_quadXLeft->setText(d.quadXLeft);
        if (m_quadXRight) m_quadXRight->setText(d.quadXRight);
        if (m_quadYBottom) m_quadYBottom->setText(d.quadYBottom);
        if (m_quadYTop) m_quadYTop->setText(d.quadYTop);
        if (m_quadQ1) m_quadQ1->setText(d.quadQ1);
        if (m_quadQ2) m_quadQ2->setText(d.quadQ2);
        if (m_quadQ3) m_quadQ3->setText(d.quadQ3);
        if (m_quadQ4) m_quadQ4->setText(d.quadQ4);
        m_quadTable->setRowCount(d.quadPoints.size());
        for (int r = 0; r < d.quadPoints.size(); ++r) {
            m_quadTable->setItem(r, 0, new QTableWidgetItem(d.quadPoints[r][0]));
            m_quadTable->setItem(r, 1, new QTableWidgetItem(d.quadPoints[r][1]));
            m_quadTable->setItem(r, 2, new QTableWidgetItem(d.quadPoints[r][2]));
        }
        setChartType(static_cast<int>(ChartType::Quadrant));
        break;

    case ChartSource::MermaidType::Sankey:
        m_sankeyTable->setRowCount(d.sankeyLinks.size());
        for (int r = 0; r < d.sankeyLinks.size(); ++r) {
            m_sankeyTable->setItem(r, 0, new QTableWidgetItem(d.sankeyLinks[r][0]));
            m_sankeyTable->setItem(r, 1, new QTableWidgetItem(d.sankeyLinks[r][1]));
            m_sankeyTable->setItem(r, 2, new QTableWidgetItem(d.sankeyLinks[r][2]));
        }
        setChartType(static_cast<int>(ChartType::Sankey));
        break;

    case ChartSource::MermaidType::Class:
    case ChartSource::MermaidType::ER:
    case ChartSource::MermaidType::Unknown:
        // Handled by prefillFromSource's raw-source fallback.
        break;
    }
}

// ============================================================
//  Pie Chart
// ============================================================

QWidget *MermaidDialog::createPiePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Title:")));
    m_pieTitle = new QLineEdit(panel);
    m_pieTitle->setPlaceholderText("My Pie Chart");
    layout->addWidget(m_pieTitle);

    layout->addWidget(new QLabel(tr("Slices:")));
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+Row", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(csvBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_pieTable, {"Label", "Value"}, {0, 1});
    });

    const int delCol = 2;
    m_pieTable = new QTableWidget(2, 3, panel);
    m_pieTable->setHorizontalHeaderLabels({"Label", "Value", "Del"});
    m_pieTable->setItem(0, 0, new QTableWidgetItem("Alpha"));
    m_pieTable->setItem(0, 1, new QTableWidgetItem("30"));
    m_pieTable->setItem(1, 0, new QTableWidgetItem("Beta"));
    m_pieTable->setItem(1, 1, new QTableWidgetItem("70"));
    m_pieTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pieTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_pieTable->setColumnWidth(delCol, 32);
    m_pieTable->verticalHeader()->setDefaultSectionSize(28);
    layout->addWidget(m_pieTable);

    addDeleteButton(m_pieTable, delCol, 0);
    addDeleteButton(m_pieTable, delCol, 1);

    layout->addStretch();

    connect(m_pieTitle, &QLineEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_pieTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_pieTable->rowCount();
        m_pieTable->insertRow(row);
        m_pieTable->setItem(row, 0, new QTableWidgetItem(""));
        m_pieTable->setItem(row, 1, new QTableWidgetItem("0"));
        addDeleteButton(m_pieTable, 2, row);
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createFlowchartPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Direction:")));
    m_fcDirection = new QComboBox(panel);
    m_fcDirection->addItem("Top-Down (TD)", "TD");
    m_fcDirection->addItem("Left-Right (LR)", "LR");
    m_fcDirection->addItem("Bottom-Top (BT)", "BT");
    m_fcDirection->addItem("Right-Left (RL)", "RL");
    layout->addWidget(m_fcDirection);

    auto *nodeGroup = new QGroupBox(tr("Nodes"), panel);
    auto *nodeLayout = new QVBoxLayout(nodeGroup);
    auto *nodeBtnLayout = new QHBoxLayout();
    auto *addNodeBtn = new QPushButton("+Node", nodeGroup);
    nodeBtnLayout->addWidget(addNodeBtn);
    nodeBtnLayout->addStretch();
    nodeLayout->addLayout(nodeBtnLayout);

    const int nodeDelCol = 3;
    m_fcNodeTable = new QTableWidget(2, 4, nodeGroup);
    m_fcNodeTable->setHorizontalHeaderLabels({"ID", "Text", "Shape", "Del"});
    m_fcNodeTable->setItem(0, 0, new QTableWidgetItem("A"));
    m_fcNodeTable->setItem(0, 1, new QTableWidgetItem("Start"));
    m_fcNodeTable->setItem(1, 0, new QTableWidgetItem("B"));
    m_fcNodeTable->setItem(1, 1, new QTableWidgetItem("Process"));
    m_fcNodeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_fcNodeTable->horizontalHeader()->setSectionResizeMode(nodeDelCol, QHeaderView::Fixed);
    m_fcNodeTable->setColumnWidth(nodeDelCol, 32);
    m_fcNodeTable->verticalHeader()->setDefaultSectionSize(28);

    auto *shapeCombo0 = new QComboBox(nodeGroup);
    shapeCombo0->addItems({"Box", "Round", "Stadium", "Diamond", "Hexagon"});
    m_fcNodeTable->setCellWidget(0, 2, shapeCombo0);
    auto *shapeCombo1 = new QComboBox(nodeGroup);
    shapeCombo1->addItems({"Box", "Round", "Stadium", "Diamond", "Hexagon"});
    shapeCombo1->setCurrentIndex(1);
    m_fcNodeTable->setCellWidget(1, 2, shapeCombo1);

    addDeleteButton(m_fcNodeTable, nodeDelCol, 0, [this](){ refreshEdgeNodeCombos(); });
    addDeleteButton(m_fcNodeTable, nodeDelCol, 1, [this](){ refreshEdgeNodeCombos(); });

    nodeLayout->addWidget(m_fcNodeTable);
    layout->addWidget(nodeGroup);

    auto *edgeGroup = new QGroupBox(tr("Edges"), panel);
    auto *edgeLayout = new QVBoxLayout(edgeGroup);
    auto *edgeBtnLayout = new QHBoxLayout();
    auto *addEdgeBtn = new QPushButton("+Edge", edgeGroup);
    edgeBtnLayout->addWidget(addEdgeBtn);
    edgeBtnLayout->addStretch();
    edgeLayout->addLayout(edgeBtnLayout);

    const int edgeDelCol = 4;
    m_fcEdgeTable = new QTableWidget(1, 5, edgeGroup);
    m_fcEdgeTable->setHorizontalHeaderLabels({"From", "To", "Label", "Arrow", "Del"});
    m_fcEdgeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_fcEdgeTable->horizontalHeader()->setSectionResizeMode(edgeDelCol, QHeaderView::Fixed);
    m_fcEdgeTable->setColumnWidth(edgeDelCol, 32);
    m_fcEdgeTable->verticalHeader()->setDefaultSectionSize(28);

    QStringList nodeIds = {"A", "B"};
    auto *fromCombo = new QComboBox(edgeGroup);
    fromCombo->addItems(nodeIds);
    m_fcEdgeTable->setCellWidget(0, 0, fromCombo);
    auto *toCombo = new QComboBox(edgeGroup);
    toCombo->addItems(nodeIds);
    toCombo->setCurrentIndex(1);
    m_fcEdgeTable->setCellWidget(0, 1, toCombo);
    m_fcEdgeTable->setItem(0, 2, new QTableWidgetItem(""));
    auto *arrowCombo = new QComboBox(edgeGroup);
    for (int i = 0; i < kArrowCount; ++i)
        arrowCombo->addItem(kArrowTypes[i].display);
    m_fcEdgeTable->setCellWidget(0, 3, arrowCombo);

    addDeleteButton(m_fcEdgeTable, edgeDelCol, 0);

    edgeLayout->addWidget(m_fcEdgeTable);
    layout->addWidget(edgeGroup);
    layout->addStretch();

    connect(m_fcDirection, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_fcNodeTable, &QTableWidget::itemChanged, this, [this]() {
        refreshEdgeNodeCombos();
        schedulePreviewUpdate();
    });
    connect(m_fcEdgeTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);

    auto onComboChange = [this]() { schedulePreviewUpdate(); };
    connect(shapeCombo0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(shapeCombo1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(fromCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(toCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(arrowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);

    connect(addNodeBtn, &QPushButton::clicked, this, [this]() {
        int row = m_fcNodeTable->rowCount();
        m_fcNodeTable->insertRow(row);
        QString id = QString(QChar('A' + row));
        m_fcNodeTable->setItem(row, 0, new QTableWidgetItem(id));
        m_fcNodeTable->setItem(row, 1, new QTableWidgetItem(""));
        auto *shapeCombo = new QComboBox(m_fcNodeTable);
        shapeCombo->addItems({"Box", "Round", "Stadium", "Diamond", "Hexagon"});
        connect(shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidDialog::schedulePreviewUpdate);
        m_fcNodeTable->setCellWidget(row, 2, shapeCombo);
        addDeleteButton(m_fcNodeTable, 3, row, [this](){ refreshEdgeNodeCombos(); });
        refreshEdgeNodeCombos();
        schedulePreviewUpdate();
    });
    connect(addEdgeBtn, &QPushButton::clicked, this, [this]() {
        int row = m_fcEdgeTable->rowCount();
        m_fcEdgeTable->insertRow(row);
        m_fcEdgeTable->setItem(row, 2, new QTableWidgetItem(""));
        addDeleteButton(m_fcEdgeTable, 4, row);
        refreshEdgeNodeCombos();
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createSequencePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Participants:")));
    auto *partBtnLayout = new QHBoxLayout();
    auto *addPartBtn = new QPushButton("+Participant", panel);
    partBtnLayout->addWidget(addPartBtn);
    partBtnLayout->addStretch();
    layout->addLayout(partBtnLayout);

    const int partDelCol = 2;
    m_seqParticipantTable = new QTableWidget(2, 3, panel);
    m_seqParticipantTable->setHorizontalHeaderLabels({"Name", "Alias", "Del"});
    m_seqParticipantTable->setItem(0, 0, new QTableWidgetItem("Alice"));
    m_seqParticipantTable->setItem(1, 0, new QTableWidgetItem("Bob"));
    m_seqParticipantTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_seqParticipantTable->horizontalHeader()->setSectionResizeMode(partDelCol, QHeaderView::Fixed);
    m_seqParticipantTable->setColumnWidth(partDelCol, 32);
    m_seqParticipantTable->verticalHeader()->setDefaultSectionSize(28);
    addDeleteButton(m_seqParticipantTable, partDelCol, 0, [this](){ refreshMessageCombos(); });
    addDeleteButton(m_seqParticipantTable, partDelCol, 1, [this](){ refreshMessageCombos(); });
    layout->addWidget(m_seqParticipantTable);

    layout->addWidget(new QLabel(tr("Messages:")));
    auto *msgBtnLayout = new QHBoxLayout();
    auto *addMsgBtn = new QPushButton("+Message", panel);
    msgBtnLayout->addWidget(addMsgBtn);
    msgBtnLayout->addStretch();
    layout->addLayout(msgBtnLayout);

    const int msgDelCol = 4;
    m_seqMessageTable = new QTableWidget(2, 5, panel);
    m_seqMessageTable->setHorizontalHeaderLabels({"From", "To", "Label", "Arrow", "Del"});
    m_seqMessageTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_seqMessageTable->horizontalHeader()->setSectionResizeMode(msgDelCol, QHeaderView::Fixed);
    m_seqMessageTable->setColumnWidth(msgDelCol, 32);
    m_seqMessageTable->verticalHeader()->setDefaultSectionSize(28);
    addDeleteButton(m_seqMessageTable, msgDelCol, 0);
    addDeleteButton(m_seqMessageTable, msgDelCol, 1);
    layout->addWidget(m_seqMessageTable);

    layout->addStretch();

    connect(m_seqParticipantTable, &QTableWidget::itemChanged, this, [this]() {
        refreshMessageCombos();
        schedulePreviewUpdate();
    });
    connect(m_seqMessageTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);

    connect(addPartBtn, &QPushButton::clicked, this, [this]() {
        int row = m_seqParticipantTable->rowCount();
        m_seqParticipantTable->insertRow(row);
        addDeleteButton(m_seqParticipantTable, 2, row, [this](){ refreshMessageCombos(); });
        refreshMessageCombos();
        schedulePreviewUpdate();
    });
    connect(addMsgBtn, &QPushButton::clicked, this, [this]() {
        int row = m_seqMessageTable->rowCount();
        m_seqMessageTable->insertRow(row);
        m_seqMessageTable->setItem(row, 2, new QTableWidgetItem(""));
        addDeleteButton(m_seqMessageTable, 4, row);
        refreshMessageCombos();
        schedulePreviewUpdate();
    });

    refreshMessageCombos();

    m_seqMessageTable->setItem(0, 2, new QTableWidgetItem("Hello"));
    auto *arrow0 = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(0, 3));
    if (arrow0) arrow0->setCurrentIndex(0);
    m_seqMessageTable->setItem(1, 2, new QTableWidgetItem("Hi"));
    auto *arrow1 = qobject_cast<QComboBox*>(m_seqMessageTable->cellWidget(1, 3));
    if (arrow1) arrow1->setCurrentIndex(1);

    return panel;
}
QWidget *MermaidDialog::createGanttPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *titleLayout = new QHBoxLayout();
    titleLayout->addWidget(new QLabel(tr("Title:")));
    m_ganttTitle = new QLineEdit(panel);
    m_ganttTitle->setPlaceholderText("Project Plan");
    titleLayout->addWidget(m_ganttTitle);
    layout->addLayout(titleLayout);

    auto *dateFormatLayout = new QHBoxLayout();
    dateFormatLayout->addWidget(new QLabel(tr("Date format:")));
    m_ganttDateFormat = new QComboBox(panel);
    m_ganttDateFormat->addItems({"YYYY-MM-DD", "DD/MM/YYYY", "MM-DD-YYYY"});
    dateFormatLayout->addWidget(m_ganttDateFormat);
    layout->addLayout(dateFormatLayout);

    m_ganttWeekend = new QCheckBox(tr("Exclude weekends"), panel);
    m_ganttWeekend->setChecked(true);
    layout->addWidget(m_ganttWeekend);

    layout->addWidget(new QLabel(tr("Tasks:")));
    auto *taskBtnLayout = new QHBoxLayout();
    auto *addTaskBtn = new QPushButton("+Task", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    taskBtnLayout->addWidget(addTaskBtn);
    taskBtnLayout->addWidget(csvBtn);
    taskBtnLayout->addStretch();
    layout->addLayout(taskBtnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_ganttTaskTable, {"ID", "Description", "Start/After", "Duration", "Status", "Section"},
                          {0, 1, 2, 3, 4, 5});
    });

    const int delCol = 6;
    m_ganttTaskTable = new QTableWidget(4, 7, panel);
    m_ganttTaskTable->setHorizontalHeaderLabels({"ID", "Description", "Start/After", "Duration", "Status", "Section", "Del"});
    m_ganttTaskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ganttTaskTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_ganttTaskTable->setColumnWidth(delCol, 32);
    m_ganttTaskTable->verticalHeader()->setDefaultSectionSize(28);
    layout->addWidget(m_ganttTaskTable);

    layout->addStretch();

    connect(m_ganttTitle, &QLineEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_ganttDateFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_ganttWeekend, &QCheckBox::toggled, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_ganttTaskTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);

    auto populateDefaultRow = [this](int row, const QString &id, const QString &desc,
                                     const QString &start, const QString &duration,
                                     const QString &status, const QString &section) {
        m_ganttTaskTable->setItem(row, 0, new QTableWidgetItem(id));
        m_ganttTaskTable->setItem(row, 1, new QTableWidgetItem(desc));
        m_ganttTaskTable->setItem(row, 2, new QTableWidgetItem(start));
        m_ganttTaskTable->setItem(row, 3, new QTableWidgetItem(duration));
        m_ganttTaskTable->setItem(row, 5, new QTableWidgetItem(section));
        auto *statusCombo = new QComboBox(m_ganttTaskTable);
        statusCombo->addItem("", "");
        statusCombo->addItem("Done", "done");
        statusCombo->addItem("Active", "active");
        statusCombo->addItem("Crit", "crit");
        statusCombo->addItem("Milestone", "milestone");
        int idx = statusCombo->findData(status);
        if (idx >= 0) statusCombo->setCurrentIndex(idx);
        m_ganttTaskTable->setCellWidget(row, 4, statusCombo);
        connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidDialog::schedulePreviewUpdate);
    };

    populateDefaultRow(0, "a1", "API design", "2026-07-01", "7d", "done", "Backend");
    populateDefaultRow(1, "a2", "DB schema", "after a1", "5d", "done", "Backend");
    populateDefaultRow(2, "a3", "Endpoints", "after a2", "14d", "active", "Frontend");
    populateDefaultRow(3, "b1", "UI components", "2026-07-10", "10d", "", "Frontend");
    for (int r = 0; r < m_ganttTaskTable->rowCount(); ++r)
        addDeleteButton(m_ganttTaskTable, delCol, r);

    connect(addTaskBtn, &QPushButton::clicked, this, [this]() {
        int row = m_ganttTaskTable->rowCount();
        m_ganttTaskTable->insertRow(row);
        m_ganttTaskTable->setItem(row, 0, new QTableWidgetItem(""));
        m_ganttTaskTable->setItem(row, 1, new QTableWidgetItem(""));
        m_ganttTaskTable->setItem(row, 2, new QTableWidgetItem(""));
        m_ganttTaskTable->setItem(row, 3, new QTableWidgetItem(""));
        m_ganttTaskTable->setItem(row, 5, new QTableWidgetItem(""));
        auto *statusCombo = new QComboBox(m_ganttTaskTable);
        statusCombo->addItem("", "");
        statusCombo->addItem("Done", "done");
        statusCombo->addItem("Active", "active");
        statusCombo->addItem("Crit", "crit");
        statusCombo->addItem("Milestone", "milestone");
        m_ganttTaskTable->setCellWidget(row, 4, statusCombo);
        connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidDialog::schedulePreviewUpdate);
        addDeleteButton(m_ganttTaskTable, 6, row);
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createClassPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Classes:")));
    m_classTable = new QTableWidget(panel);
    m_classTable->setColumnCount(2);
    m_classTable->setHorizontalHeaderLabels({tr("Name"), tr("Type")});
    m_classTable->horizontalHeader()->setStretchLastSection(true);
    m_classTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_classTable);

    auto *classBtns = new QHBoxLayout;
    auto *addClassBtn = new QPushButton(tr("Add"));
    auto *removeClassBtn = new QPushButton(tr("Remove"));
    classBtns->addWidget(addClassBtn);
    classBtns->addWidget(removeClassBtn);
    layout->addLayout(classBtns);

    layout->addWidget(new QLabel(tr("Fields (for selected class):")));
    m_classFieldTable = new QTableWidget(panel);
    m_classFieldTable->setColumnCount(4);
    m_classFieldTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Visibility"), tr("Static")});
    m_classFieldTable->horizontalHeader()->setStretchLastSection(true);
    m_classFieldTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_classFieldTable);

    auto *fieldBtns = new QHBoxLayout;
    auto *addFieldBtn = new QPushButton(tr("Add"));
    auto *removeFieldBtn = new QPushButton(tr("Remove"));
    fieldBtns->addWidget(addFieldBtn);
    fieldBtns->addWidget(removeFieldBtn);
    layout->addLayout(fieldBtns);

    layout->addWidget(new QLabel(tr("Methods (for selected class):")));
    m_classMethodTable = new QTableWidget(panel);
    m_classMethodTable->setColumnCount(4);
    m_classMethodTable->setHorizontalHeaderLabels({tr("Name"), tr("Return Type"), tr("Parameters"), tr("Visibility")});
    m_classMethodTable->horizontalHeader()->setStretchLastSection(true);
    m_classMethodTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_classMethodTable);

    auto *methodBtns = new QHBoxLayout;
    auto *addMethodBtn = new QPushButton(tr("Add"));
    auto *removeMethodBtn = new QPushButton(tr("Remove"));
    methodBtns->addWidget(addMethodBtn);
    methodBtns->addWidget(removeMethodBtn);
    layout->addLayout(methodBtns);

    layout->addWidget(new QLabel(tr("Relations:")));
    m_classRelationTable = new QTableWidget(panel);
    m_classRelationTable->setColumnCount(4);
    m_classRelationTable->setHorizontalHeaderLabels({tr("From"), tr("To"), tr("Type"), tr("Label")});
    m_classRelationTable->horizontalHeader()->setStretchLastSection(true);
    m_classRelationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_classRelationTable);

    auto *relBtns = new QHBoxLayout;
    auto *addRelBtn = new QPushButton(tr("Add"));
    auto *removeRelBtn = new QPushButton(tr("Remove"));
    relBtns->addWidget(addRelBtn);
    relBtns->addWidget(removeRelBtn);
    layout->addLayout(relBtns);

    layout->addStretch();

    connect(m_classTable, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { Q_UNUSED(row) saveCurrentClassData(); m_lastClassRow = row; loadClassData(row); schedulePreviewUpdate(); });
    connect(m_classTable, &QTableWidget::cellChanged, this, [this]() { refreshClassRelCombos(); schedulePreviewUpdate(); });

    connect(addClassBtn, &QPushButton::clicked, this, [this]() {
        int row = m_classTable->rowCount();
        m_classTable->insertRow(row);
        m_classTable->setItem(row, 0, new QTableWidgetItem(tr("MyClass")));
        auto *typeCombo = new QComboBox;
        typeCombo->addItems({"Class", "Interface", "Abstract", "Enumeration"});
        m_classTable->setCellWidget(row, 1, typeCombo);
        m_classTable->setCurrentCell(row, 0);
        refreshClassRelCombos();
        schedulePreviewUpdate();
    });
    connect(removeClassBtn, &QPushButton::clicked, this, [this]() {
        int row = m_classTable->currentRow();
        if (row < 0) return;
        m_classData.remove(row);
        m_classTable->removeRow(row);
        QMap<int, ClassData> newData;
        for (auto it = m_classData.constBegin(); it != m_classData.constEnd(); ++it) {
            if (it.key() > row) newData[it.key() - 1] = it.value();
            else if (it.key() < row) newData[it.key()] = it.value();
        }
        m_classData = newData;
        if (m_lastClassRow == row) { m_lastClassRow = -1; m_classFieldTable->setRowCount(0); m_classMethodTable->setRowCount(0); }
        else if (m_lastClassRow > row) m_lastClassRow--;
        refreshClassRelCombos();
        schedulePreviewUpdate();
    });

    connect(addFieldBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentClassData();
        int row = m_classFieldTable->rowCount();
        m_classFieldTable->insertRow(row);
        m_classFieldTable->setItem(row, 0, new QTableWidgetItem);
        m_classFieldTable->setItem(row, 1, new QTableWidgetItem);
        auto *visCombo = new QComboBox;
        visCombo->addItems({"+", "-", "#", "~"});
        m_classFieldTable->setCellWidget(row, 2, visCombo);
        auto *staticCheck = new QCheckBox;
        m_classFieldTable->setCellWidget(row, 3, staticCheck);
        int classRow = m_classTable->currentRow();
        if (classRow >= 0) {
            QMap<QString, QString> field;
            field["name"] = QString(); field["type"] = QString(); field["visibility"] = "+"; field["static"] = "false";
            m_classData[classRow].fields.append(field);
        }
        schedulePreviewUpdate();
    });
    connect(removeFieldBtn, &QPushButton::clicked, this, [this]() {
        int row = m_classFieldTable->currentRow();
        if (row < 0) return;
        m_classFieldTable->removeRow(row);
        int classRow = m_classTable->currentRow();
        if (classRow >= 0) {
            auto &fields = m_classData[classRow].fields;
            if (row < fields.size()) fields.removeAt(row);
        }
        schedulePreviewUpdate();
    });

    connect(addMethodBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentClassData();
        int row = m_classMethodTable->rowCount();
        m_classMethodTable->insertRow(row);
        m_classMethodTable->setItem(row, 0, new QTableWidgetItem);
        m_classMethodTable->setItem(row, 1, new QTableWidgetItem);
        m_classMethodTable->setItem(row, 2, new QTableWidgetItem);
        auto *visCombo = new QComboBox;
        visCombo->addItems({"+", "-", "#", "~"});
        m_classMethodTable->setCellWidget(row, 3, visCombo);
        int classRow = m_classTable->currentRow();
        if (classRow >= 0) {
            QMap<QString, QString> method;
            method["name"] = QString(); method["returnType"] = QString(); method["parameters"] = QString(); method["visibility"] = "+";
            m_classData[classRow].methods.append(method);
        }
        schedulePreviewUpdate();
    });
    connect(removeMethodBtn, &QPushButton::clicked, this, [this]() {
        int row = m_classMethodTable->currentRow();
        if (row < 0) return;
        m_classMethodTable->removeRow(row);
        int classRow = m_classTable->currentRow();
        if (classRow >= 0) {
            auto &methods = m_classData[classRow].methods;
            if (row < methods.size()) methods.removeAt(row);
        }
        schedulePreviewUpdate();
    });

    connect(addRelBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentClassData();
        int row = m_classRelationTable->rowCount();
        m_classRelationTable->insertRow(row);
        QStringList names;
        for (int r = 0; r < m_classTable->rowCount(); ++r) {
            auto *item = m_classTable->item(r, 0);
            if (item && !item->text().isEmpty()) names.append(item->text());
        }
        auto *fromCombo = new QComboBox; fromCombo->addItems(names);
        auto *toCombo = new QComboBox; toCombo->addItems(names);
        auto *typeCombo = new QComboBox;
        typeCombo->addItems({"-->", "<|--", "..|>", "*--", "o--"});
        m_classRelationTable->setCellWidget(row, 0, fromCombo);
        m_classRelationTable->setCellWidget(row, 1, toCombo);
        m_classRelationTable->setCellWidget(row, 2, typeCombo);
        m_classRelationTable->setItem(row, 3, new QTableWidgetItem);
        schedulePreviewUpdate();
    });
    connect(removeRelBtn, &QPushButton::clicked, this, [this]() {
        int row = m_classRelationTable->currentRow();
        if (row < 0) return;
        m_classRelationTable->removeRow(row);
        schedulePreviewUpdate();
    });

    // Default data
    m_classTable->insertRow(0);
    m_classTable->setItem(0, 0, new QTableWidgetItem("Animal"));
    auto *type0 = new QComboBox; type0->addItems({"Class", "Interface", "Abstract", "Enumeration"});
    m_classTable->setCellWidget(0, 1, type0);
    m_classTable->insertRow(1);
    m_classTable->setItem(1, 0, new QTableWidgetItem("Dog"));
    auto *type1 = new QComboBox; type1->addItems({"Class", "Interface", "Abstract", "Enumeration"});
    m_classTable->setCellWidget(1, 1, type1);

    ClassData animalData;
    QMap<QString, QString> f1; f1["name"] = "name"; f1["type"] = "string"; f1["visibility"] = "+"; f1["static"] = "false"; animalData.fields.append(f1);
    QMap<QString, QString> f2; f2["name"] = "age"; f2["type"] = "int"; f2["visibility"] = "+"; f2["static"] = "false"; animalData.fields.append(f2);
    QMap<QString, QString> m1; m1["name"] = "move"; m1["returnType"] = "void"; m1["parameters"] = ""; m1["visibility"] = "+"; animalData.methods.append(m1);
    m_classData[0] = animalData;

    ClassData dogData;
    QMap<QString, QString> df1; df1["name"] = "breed"; df1["type"] = "string"; df1["visibility"] = "+"; df1["static"] = "false"; dogData.fields.append(df1);
    QMap<QString, QString> dm1; dm1["name"] = "bark"; dm1["returnType"] = "void"; dm1["parameters"] = ""; dm1["visibility"] = "+"; dogData.methods.append(dm1);
    m_classData[1] = dogData;

    m_classRelationTable->insertRow(0);
    QStringList names = {"Animal", "Dog"};
    auto *fromCombo = new QComboBox; fromCombo->addItems(names); fromCombo->setCurrentText("Animal");
    auto *toCombo = new QComboBox; toCombo->addItems(names); toCombo->setCurrentText("Dog");
    auto *typeCombo = new QComboBox; typeCombo->addItems({"-->", "<|--", "..|>", "*--", "o--"});
    typeCombo->setCurrentText("<|--");
    m_classRelationTable->setCellWidget(0, 0, fromCombo);
    m_classRelationTable->setCellWidget(0, 1, toCombo);
    m_classRelationTable->setCellWidget(0, 2, typeCombo);
    m_classRelationTable->setItem(0, 3, new QTableWidgetItem("extends"));

    m_classTable->setCurrentCell(0, 0);

    return panel;
}
QWidget *MermaidDialog::createERPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Entities:")));
    m_erEntityTable = new QTableWidget(panel);
    m_erEntityTable->setColumnCount(1);
    m_erEntityTable->setHorizontalHeaderLabels({tr("Name")});
    m_erEntityTable->horizontalHeader()->setStretchLastSection(true);
    m_erEntityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_erEntityTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_erEntityTable);

    auto *entityBtns = new QHBoxLayout;
    auto *addEntityBtn = new QPushButton(tr("Add"));
    auto *removeEntityBtn = new QPushButton(tr("Remove"));
    entityBtns->addWidget(addEntityBtn);
    entityBtns->addWidget(removeEntityBtn);
    layout->addLayout(entityBtns);

    layout->addWidget(new QLabel(tr("Attributes (for selected entity):")));
    m_erAttributeTable = new QTableWidget(panel);
    m_erAttributeTable->setColumnCount(3);
    m_erAttributeTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Key")});
    m_erAttributeTable->horizontalHeader()->setStretchLastSection(true);
    m_erAttributeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_erAttributeTable);

    auto *attrBtns = new QHBoxLayout;
    auto *addAttrBtn = new QPushButton(tr("Add"));
    auto *removeAttrBtn = new QPushButton(tr("Remove"));
    attrBtns->addWidget(addAttrBtn);
    attrBtns->addWidget(removeAttrBtn);
    layout->addLayout(attrBtns);

    layout->addWidget(new QLabel(tr("Relations:")));
    m_erRelationTable = new QTableWidget(panel);
    m_erRelationTable->setColumnCount(4);
    m_erRelationTable->setHorizontalHeaderLabels({tr("From"), tr("To"), tr("Type"), tr("Label")});
    m_erRelationTable->horizontalHeader()->setStretchLastSection(true);
    m_erRelationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_erRelationTable);

    auto *relBtns = new QHBoxLayout;
    auto *addRelBtn = new QPushButton(tr("Add"));
    auto *removeRelBtn = new QPushButton(tr("Remove"));
    relBtns->addWidget(addRelBtn);
    relBtns->addWidget(removeRelBtn);
    layout->addLayout(relBtns);

    layout->addStretch();

    connect(m_erEntityTable, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { Q_UNUSED(row) saveCurrentERAttrs(); m_lastEntityRow = row; loadERAttrs(row); schedulePreviewUpdate(); });
    connect(m_erEntityTable, &QTableWidget::cellChanged, this, [this]() { refreshERRelCombos(); schedulePreviewUpdate(); });

    connect(addEntityBtn, &QPushButton::clicked, this, [this]() {
        int row = m_erEntityTable->rowCount();
        m_erEntityTable->insertRow(row);
        m_erEntityTable->setItem(row, 0, new QTableWidgetItem(tr("Entity")));
        m_erEntityTable->setCurrentCell(row, 0);
        refreshERRelCombos();
        schedulePreviewUpdate();
    });
    connect(removeEntityBtn, &QPushButton::clicked, this, [this]() {
        int row = m_erEntityTable->currentRow();
        if (row < 0) return;
        m_erEntityAttrs.remove(row);
        m_erEntityTable->removeRow(row);
        QMap<int, QList<QMap<QString, QString>>> newMap;
        for (auto it = m_erEntityAttrs.constBegin(); it != m_erEntityAttrs.constEnd(); ++it) {
            if (it.key() > row) newMap[it.key() - 1] = it.value();
            else if (it.key() < row) newMap[it.key()] = it.value();
        }
        m_erEntityAttrs = newMap;
        if (m_lastEntityRow == row) { m_lastEntityRow = -1; m_erAttributeTable->setRowCount(0); }
        else if (m_lastEntityRow > row) m_lastEntityRow--;
        refreshERRelCombos();
        schedulePreviewUpdate();
    });

    connect(addAttrBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentERAttrs();
        int row = m_erAttributeTable->rowCount();
        m_erAttributeTable->insertRow(row);
        m_erAttributeTable->setItem(row, 0, new QTableWidgetItem);
        m_erAttributeTable->setItem(row, 1, new QTableWidgetItem);
        auto *keyCombo = new QComboBox; keyCombo->addItems({"", "PK", "FK"});
        m_erAttributeTable->setCellWidget(row, 2, keyCombo);
        int entityRow = m_erEntityTable->currentRow();
        if (entityRow >= 0) {
            auto &attrs = m_erEntityAttrs[entityRow];
            QMap<QString, QString> newAttr;
            newAttr["name"] = QString(); newAttr["type"] = QString(); newAttr["key"] = QString();
            attrs.append(newAttr);
        }
        schedulePreviewUpdate();
    });
    connect(removeAttrBtn, &QPushButton::clicked, this, [this]() {
        int row = m_erAttributeTable->currentRow();
        if (row < 0) return;
        m_erAttributeTable->removeRow(row);
        int entityRow = m_erEntityTable->currentRow();
        if (entityRow >= 0) {
            auto &attrs = m_erEntityAttrs[entityRow];
            if (row < attrs.size()) attrs.removeAt(row);
        }
        schedulePreviewUpdate();
    });

    connect(addRelBtn, &QPushButton::clicked, this, [this]() {
        saveCurrentERAttrs();
        int row = m_erRelationTable->rowCount();
        m_erRelationTable->insertRow(row);
        QStringList names;
        for (int r = 0; r < m_erEntityTable->rowCount(); ++r) {
            auto *item = m_erEntityTable->item(r, 0);
            if (item && !item->text().isEmpty()) names.append(item->text());
        }
        auto *fromCombo = new QComboBox; fromCombo->addItems(names);
        auto *toCombo = new QComboBox; toCombo->addItems(names);
        auto *typeCombo = new QComboBox; typeCombo->addItems({"||--o{", "||--||", "}|--||", "}|--o{"});
        m_erRelationTable->setCellWidget(row, 0, fromCombo);
        m_erRelationTable->setCellWidget(row, 1, toCombo);
        m_erRelationTable->setCellWidget(row, 2, typeCombo);
        m_erRelationTable->setItem(row, 3, new QTableWidgetItem);
        schedulePreviewUpdate();
    });
    connect(removeRelBtn, &QPushButton::clicked, this, [this]() {
        int row = m_erRelationTable->currentRow();
        if (row < 0) return;
        m_erRelationTable->removeRow(row);
        schedulePreviewUpdate();
    });

    // Default data
    m_erEntityTable->insertRow(0);
    m_erEntityTable->setItem(0, 0, new QTableWidgetItem("USER"));
    m_erEntityTable->insertRow(1);
    m_erEntityTable->setItem(1, 0, new QTableWidgetItem("POST"));

    QList<QMap<QString, QString>> userAttrs;
    QMap<QString, QString> u1; u1["name"] = "id"; u1["type"] = "int"; u1["key"] = "PK"; userAttrs.append(u1);
    QMap<QString, QString> u2; u2["name"] = "email"; u2["type"] = "string"; u2["key"] = ""; userAttrs.append(u2);
    QMap<QString, QString> u3; u3["name"] = "name"; u3["type"] = "string"; u3["key"] = ""; userAttrs.append(u3);
    m_erEntityAttrs[0] = userAttrs;

    QList<QMap<QString, QString>> postAttrs;
    QMap<QString, QString> p1; p1["name"] = "id"; p1["type"] = "int"; p1["key"] = "PK"; postAttrs.append(p1);
    QMap<QString, QString> p2; p2["name"] = "title"; p2["type"] = "string"; p2["key"] = ""; postAttrs.append(p2);
    QMap<QString, QString> p3; p3["name"] = "user_id"; p3["type"] = "int"; p3["key"] = "FK"; postAttrs.append(p3);
    m_erEntityAttrs[1] = postAttrs;

    m_erRelationTable->insertRow(0);
    QStringList enames = {"USER", "POST"};
    auto *fromCombo = new QComboBox; fromCombo->addItems(enames); fromCombo->setCurrentText("USER");
    auto *toCombo = new QComboBox; toCombo->addItems(enames); toCombo->setCurrentText("POST");
    auto *typeCombo = new QComboBox; typeCombo->addItems({"||--o{", "||--||", "}|--||", "}|--o{"});
    m_erRelationTable->setCellWidget(0, 0, fromCombo);
    m_erRelationTable->setCellWidget(0, 1, toCombo);
    m_erRelationTable->setCellWidget(0, 2, typeCombo);
    m_erRelationTable->setItem(0, 3, new QTableWidgetItem("writes"));

    m_erEntityTable->setCurrentCell(0, 0);

    return panel;
}
QWidget *MermaidDialog::createStatePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("States:")));
    auto *stateBtnLayout = new QHBoxLayout();
    auto *addStateBtn = new QPushButton("+State", panel);
    stateBtnLayout->addWidget(addStateBtn);
    stateBtnLayout->addStretch();
    layout->addLayout(stateBtnLayout);

    const int stateDelCol = 2;
    m_stateTable = new QTableWidget(4, 3, panel);
    m_stateTable->setHorizontalHeaderLabels({"Name", "Description", "Del"});
    m_stateTable->setItem(0, 0, new QTableWidgetItem("[*]"));
    m_stateTable->setItem(1, 0, new QTableWidgetItem("Idle"));
    m_stateTable->setItem(2, 0, new QTableWidgetItem("Processing"));
    m_stateTable->setItem(3, 0, new QTableWidgetItem("Done"));
    m_stateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stateTable->horizontalHeader()->setSectionResizeMode(stateDelCol, QHeaderView::Fixed);
    m_stateTable->setColumnWidth(stateDelCol, 32);
    m_stateTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_stateTable->rowCount(); ++r)
        addDeleteButton(m_stateTable, stateDelCol, r, [this](){ refreshStateCombos(); });
    layout->addWidget(m_stateTable);

    layout->addWidget(new QLabel(tr("Transitions:")));
    auto *transBtnLayout = new QHBoxLayout();
    auto *addTransBtn = new QPushButton("+Transition", panel);
    transBtnLayout->addWidget(addTransBtn);
    transBtnLayout->addStretch();
    layout->addLayout(transBtnLayout);

    const int transDelCol = 4;
    m_stateTransitionTable = new QTableWidget(4, 5, panel);
    m_stateTransitionTable->setHorizontalHeaderLabels({"From", "To", "Label", "Section", "Del"});
    m_stateTransitionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stateTransitionTable->horizontalHeader()->setSectionResizeMode(transDelCol, QHeaderView::Fixed);
    m_stateTransitionTable->setColumnWidth(transDelCol, 32);
    m_stateTransitionTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_stateTransitionTable->rowCount(); ++r)
        addDeleteButton(m_stateTransitionTable, transDelCol, r);
    layout->addWidget(m_stateTransitionTable);

    layout->addStretch();

    connect(m_stateTable, &QTableWidget::itemChanged, this, [this]() { refreshStateCombos(); schedulePreviewUpdate(); });
    connect(m_stateTransitionTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);

    connect(addStateBtn, &QPushButton::clicked, this, [this]() {
        int row = m_stateTable->rowCount();
        m_stateTable->insertRow(row);
        addDeleteButton(m_stateTable, 2, row, [this](){ refreshStateCombos(); });
        refreshStateCombos();
        schedulePreviewUpdate();
    });
    connect(addTransBtn, &QPushButton::clicked, this, [this]() {
        int row = m_stateTransitionTable->rowCount();
        m_stateTransitionTable->insertRow(row);
        m_stateTransitionTable->setItem(row, 3, new QTableWidgetItem(QString()));
        addDeleteButton(m_stateTransitionTable, 4, row);
        refreshStateCombos();
        schedulePreviewUpdate();
    });

    refreshStateCombos();

    return panel;
}
QWidget *MermaidDialog::createMindmapPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    m_mindmapTree = new QTreeWidget(panel);
    m_mindmapTree->setHeaderLabel(tr("Text"));
    m_mindmapTree->setColumnCount(1);
    layout->addWidget(m_mindmapTree);

    auto *treeButtons = new QHBoxLayout;
    auto *addChildBtn = new QPushButton(tr("Add Child"));
    auto *addSiblingBtn = new QPushButton(tr("Add Sibling"));
    auto *deleteBtn = new QPushButton(tr("Delete"));
    treeButtons->addWidget(addChildBtn);
    treeButtons->addWidget(addSiblingBtn);
    treeButtons->addWidget(deleteBtn);
    layout->addLayout(treeButtons);

    connect(addChildBtn, &QPushButton::clicked, this, [this]() {
        auto *parent = m_mindmapTree->currentItem();
        if (!parent) return;
        auto *child = new QTreeWidgetItem(parent);
        child->setText(0, tr("New node"));
        parent->setExpanded(true);
        m_mindmapTree->setCurrentItem(child);
        schedulePreviewUpdate();
    });
    connect(addSiblingBtn, &QPushButton::clicked, this, [this]() {
        auto *current = m_mindmapTree->currentItem();
        if (!current) return;
        auto *parent = current->parent();
        auto *sibling = new QTreeWidgetItem(parent ? parent : m_mindmapTree->invisibleRootItem());
        sibling->setText(0, tr("New node"));
        m_mindmapTree->setCurrentItem(sibling);
        schedulePreviewUpdate();
    });
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        auto *current = m_mindmapTree->currentItem();
        if (!current || current == m_mindmapTree->topLevelItem(0)) return;
        auto *parent = current->parent();
        if (parent) parent->removeChild(current);
        else m_mindmapTree->takeTopLevelItem(m_mindmapTree->indexOfTopLevelItem(current));
        schedulePreviewUpdate();
    });

    // Default data
    auto *root = new QTreeWidgetItem(m_mindmapTree);
    root->setText(0, tr("Central Idea"));
    root->setExpanded(true);
    auto *idea1 = new QTreeWidgetItem(root);
    idea1->setText(0, tr("Idea 1"));
    auto *subIdea = new QTreeWidgetItem(idea1);
    subIdea->setText(0, tr("Sub-idea"));
    auto *idea2 = new QTreeWidgetItem(root);
    idea2->setText(0, tr("Idea 2"));
    m_mindmapTree->setCurrentItem(root);

    connect(m_mindmapTree, &QTreeWidget::itemChanged, this, [this]() { schedulePreviewUpdate(); });

    return panel;
}
QWidget *MermaidDialog::createTimelinePanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Title:")));
    m_timelineTitle = new QLineEdit(panel);
    m_timelineTitle->setPlaceholderText("My Timeline");
    layout->addWidget(m_timelineTitle);

    layout->addWidget(new QLabel(tr("Entries (Section, Event):")));
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+Row", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(csvBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_timelineTable, {"Section", "Event"}, {0, 1});
    });

    const int delCol = 2;
    m_timelineTable = new QTableWidget(3, 3, panel);
    m_timelineTable->setHorizontalHeaderLabels({"Section", "Event", "Del"});
    m_timelineTable->setItem(0, 0, new QTableWidgetItem("Q1 2026"));
    m_timelineTable->setItem(0, 1, new QTableWidgetItem("Launch v1.0"));
    m_timelineTable->setItem(1, 0, new QTableWidgetItem("Q2 2026"));
    m_timelineTable->setItem(1, 1, new QTableWidgetItem("Template library"));
    m_timelineTable->setItem(2, 0, new QTableWidgetItem("Q2 2026"));
    m_timelineTable->setItem(2, 1, new QTableWidgetItem("VS Code extension"));
    m_timelineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_timelineTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_timelineTable->setColumnWidth(delCol, 32);
    m_timelineTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_timelineTable->rowCount(); ++r)
        addDeleteButton(m_timelineTable, delCol, r);
    layout->addWidget(m_timelineTable);

    layout->addStretch();

    connect(m_timelineTitle, &QLineEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_timelineTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_timelineTable->rowCount();
        m_timelineTable->insertRow(row);
        addDeleteButton(m_timelineTable, 2, row);
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createJourneyPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Title:")));
    m_journeyTitle = new QLineEdit(panel);
    m_journeyTitle->setPlaceholderText("My Day");
    layout->addWidget(m_journeyTitle);

    layout->addWidget(new QLabel(tr("Tasks:")));
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+Row", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(csvBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_journeyTable, {"Section", "Task Name", "Score", "Actors"}, {0, 1, 2, 3});
    });

    const int delCol = 4;
    m_journeyTable = new QTableWidget(4, 5, panel);
    m_journeyTable->setHorizontalHeaderLabels({"Section", "Task Name", "Score", "Actors", "Del"});
    m_journeyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_journeyTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_journeyTable->setColumnWidth(delCol, 32);
    m_journeyTable->verticalHeader()->setDefaultSectionSize(28);

    auto setRow = [&](int row, const QString &section, const QString &task, int score, const QString &actors) {
        m_journeyTable->setItem(row, 0, new QTableWidgetItem(section));
        m_journeyTable->setItem(row, 1, new QTableWidgetItem(task));
        auto *spin = new QSpinBox(m_journeyTable);
        spin->setRange(1, 7);
        spin->setValue(score);
        m_journeyTable->setCellWidget(row, 2, spin);
        m_journeyTable->setItem(row, 3, new QTableWidgetItem(actors));
    };
    setRow(0, "Morning", "Wake up", 5, "Me");
    setRow(1, "Morning", "Shower", 3, "Me");
    setRow(2, "Work", "Coding", 7, "Me, Team");
    setRow(3, "Work", "Meetings", 2, "Me, Boss");
    for (int r = 0; r < m_journeyTable->rowCount(); ++r)
        addDeleteButton(m_journeyTable, delCol, r);
    layout->addWidget(m_journeyTable);

    layout->addStretch();

    connect(m_journeyTitle, &QLineEdit::textChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_journeyTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_journeyTable->rowCount();
        m_journeyTable->insertRow(row);
        addDeleteButton(m_journeyTable, 4, row);
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createQuadrantPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Title:")));
    m_quadTitle = new QLineEdit(panel);
    m_quadTitle->setPlaceholderText("Chart Title");
    m_quadTitle->setText("Reach and Impact");
    layout->addWidget(m_quadTitle);

    layout->addWidget(new QLabel(tr("Axes:")));
    auto *axisGrid = new QGridLayout();
    axisGrid->addWidget(new QLabel(tr("X-axis left label:")), 0, 0);
    m_quadXLeft = new QLineEdit(panel);
    m_quadXLeft->setText("Low Reach");
    axisGrid->addWidget(m_quadXLeft, 0, 1);
    axisGrid->addWidget(new QLabel(tr("X-axis right label:")), 0, 2);
    m_quadXRight = new QLineEdit(panel);
    m_quadXRight->setText("High Reach");
    axisGrid->addWidget(m_quadXRight, 0, 3);
    axisGrid->addWidget(new QLabel(tr("Y-axis bottom label:")), 1, 0);
    m_quadYBottom = new QLineEdit(panel);
    m_quadYBottom->setText("Low Impact");
    axisGrid->addWidget(m_quadYBottom, 1, 1);
    axisGrid->addWidget(new QLabel(tr("Y-axis top label:")), 1, 2);
    m_quadYTop = new QLineEdit(panel);
    m_quadYTop->setText("High Impact");
    axisGrid->addWidget(m_quadYTop, 1, 3);
    layout->addLayout(axisGrid);

    layout->addWidget(new QLabel(tr("Quadrant labels:")));
    auto *qGrid = new QGridLayout();
    qGrid->addWidget(new QLabel(tr("Q1 (top-right):")), 0, 0);
    m_quadQ1 = new QLineEdit(panel); m_quadQ1->setText("Quick Wins");
    qGrid->addWidget(m_quadQ1, 0, 1);
    qGrid->addWidget(new QLabel(tr("Q2 (top-left):")), 1, 0);
    m_quadQ2 = new QLineEdit(panel); m_quadQ2->setText("Big Bets");
    qGrid->addWidget(m_quadQ2, 1, 1);
    qGrid->addWidget(new QLabel(tr("Q3 (bottom-left):")), 2, 0);
    m_quadQ3 = new QLineEdit(panel); m_quadQ3->setText("Time Sinks");
    qGrid->addWidget(m_quadQ3, 2, 1);
    qGrid->addWidget(new QLabel(tr("Q4 (bottom-right):")), 3, 0);
    m_quadQ4 = new QLineEdit(panel); m_quadQ4->setText("Thankless Tasks");
    qGrid->addWidget(m_quadQ4, 3, 1);
    layout->addLayout(qGrid);

    layout->addWidget(new QLabel(tr("Points (Label, X 0-1, Y 0-1):")));
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+Row", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(csvBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_quadTable, {"Label", "X", "Y"}, {0, 1, 2});
    });

    const int delCol = 3;
    m_quadTable = new QTableWidget(3, 4, panel);
    m_quadTable->setHorizontalHeaderLabels({"Label", "X", "Y", "Del"});
    m_quadTable->setItem(0, 0, new QTableWidgetItem("Feature A"));
    m_quadTable->setItem(0, 1, new QTableWidgetItem("0.3"));
    m_quadTable->setItem(0, 2, new QTableWidgetItem("0.8"));
    m_quadTable->setItem(1, 0, new QTableWidgetItem("Feature B"));
    m_quadTable->setItem(1, 1, new QTableWidgetItem("0.8"));
    m_quadTable->setItem(1, 2, new QTableWidgetItem("0.9"));
    m_quadTable->setItem(2, 0, new QTableWidgetItem("Feature C"));
    m_quadTable->setItem(2, 1, new QTableWidgetItem("0.1"));
    m_quadTable->setItem(2, 2, new QTableWidgetItem("0.2"));
    m_quadTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_quadTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_quadTable->setColumnWidth(delCol, 32);
    m_quadTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_quadTable->rowCount(); ++r)
        addDeleteButton(m_quadTable, delCol, r);
    layout->addWidget(m_quadTable);

    layout->addStretch();

    auto triggerUpdate = [this]() { schedulePreviewUpdate(); };
    connect(m_quadTitle, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadXLeft, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadXRight, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadYBottom, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadYTop, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadQ1, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadQ2, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadQ3, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadQ4, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_quadTable, &QTableWidget::itemChanged, this, triggerUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_quadTable->rowCount();
        m_quadTable->insertRow(row);
        addDeleteButton(m_quadTable, 3, row);
    });

    return panel;
}
QWidget *MermaidDialog::createSankeyPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel(tr("Links (Source, Target, Value):")));
    auto *btnLayout = new QHBoxLayout();
    auto *addBtn = new QPushButton("+Row", panel);
    auto *csvBtn = new QPushButton("CSV Import", panel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(csvBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(csvBtn, &QPushButton::clicked, this, [this]() {
        csvImportForChart(m_sankeyTable, {"Source", "Target", "Value"}, {0, 1, 2});
    });

    const int delCol = 3;
    m_sankeyTable = new QTableWidget(3, 4, panel);
    m_sankeyTable->setHorizontalHeaderLabels({"Source", "Target", "Value", "Del"});
    m_sankeyTable->setItem(0, 0, new QTableWidgetItem("Revenue"));
    m_sankeyTable->setItem(0, 1, new QTableWidgetItem("Product Sales"));
    m_sankeyTable->setItem(0, 2, new QTableWidgetItem("600"));
    m_sankeyTable->setItem(1, 0, new QTableWidgetItem("Revenue"));
    m_sankeyTable->setItem(1, 1, new QTableWidgetItem("Services"));
    m_sankeyTable->setItem(1, 2, new QTableWidgetItem("300"));
    m_sankeyTable->setItem(2, 0, new QTableWidgetItem("Product Sales"));
    m_sankeyTable->setItem(2, 1, new QTableWidgetItem("COGS"));
    m_sankeyTable->setItem(2, 2, new QTableWidgetItem("250"));
    m_sankeyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sankeyTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_sankeyTable->setColumnWidth(delCol, 32);
    m_sankeyTable->verticalHeader()->setDefaultSectionSize(28);
    for (int r = 0; r < m_sankeyTable->rowCount(); ++r)
        addDeleteButton(m_sankeyTable, delCol, r);
    layout->addWidget(m_sankeyTable);

    layout->addStretch();

    connect(m_sankeyTable, &QTableWidget::itemChanged, this, &MermaidDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_sankeyTable->rowCount();
        m_sankeyTable->insertRow(row);
        addDeleteButton(m_sankeyTable, 3, row);
        schedulePreviewUpdate();
    });

    return panel;
}
QWidget *MermaidDialog::createGitGraphPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *repoRow = new QHBoxLayout;
    m_gitRepoPath = new QLineEdit(panel);
    m_gitRepoPath->setPlaceholderText(tr("Path to a git repository..."));
    auto *browseBtn = new QPushButton(tr("&Browse..."), panel);
    repoRow->addWidget(m_gitRepoPath, 1);
    repoRow->addWidget(browseBtn);
    layout->addLayout(repoRow);

    m_gitLimitCombo = new QComboBox(panel);
    m_gitLimitCombo->addItem(tr("All commits"),
                             static_cast<int>(GitGraphBuilder::Options::Limit::All));
    m_gitLimitCombo->addItem(tr("Single branch"),
                             static_cast<int>(GitGraphBuilder::Options::Limit::Branch));
    m_gitLimitCombo->addItem(tr("Date range"),
                             static_cast<int>(GitGraphBuilder::Options::Limit::Dates));
    m_gitLimitCombo->addItem(tr("Branch + date range"),
                             static_cast<int>(GitGraphBuilder::Options::Limit::BranchAndDates));
    layout->addWidget(m_gitLimitCombo);

    layout->addWidget(new QLabel(tr("Branch:")));
    m_gitBranchCombo = new QComboBox(panel);
    layout->addWidget(m_gitBranchCombo);

    auto *dateRow = new QHBoxLayout;
    m_gitFromDate = new QDateEdit(panel);
    m_gitFromDate->setCalendarPopup(true);
    m_gitFromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_gitToDate = new QDateEdit(panel);
    m_gitToDate->setCalendarPopup(true);
    m_gitToDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateRow->addWidget(new QLabel(tr("From:")));
    dateRow->addWidget(m_gitFromDate);
    dateRow->addWidget(new QLabel(tr("To:")));
    dateRow->addWidget(m_gitToDate);
    layout->addLayout(dateRow);

    auto *capRow = new QHBoxLayout;
    m_gitMaxCommits = new QSpinBox(panel);
    m_gitMaxCommits->setRange(1, 2000);
    m_gitMaxCommits->setValue(50);
    m_gitMaxCommits->setSuffix(QStringLiteral(" commits"));
    m_gitNoLimit = new QCheckBox(tr("No commit limit"), panel);
    capRow->addWidget(new QLabel(tr("Max:")));
    capRow->addWidget(m_gitMaxCommits);
    capRow->addWidget(m_gitNoLimit);
    capRow->addStretch();
    layout->addLayout(capRow);

    m_gitStatus = new QLabel(panel);
    m_gitStatus->setWordWrap(true);
    m_gitStatus->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_gitStatus);

    layout->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Git Repository"),
            m_gitRepoPath->text().isEmpty() ? QDir::homePath()
                                            : m_gitRepoPath->text());
        if (!dir.isEmpty())
            loadGitRepo(dir);
    });
    connect(m_gitRepoPath, &QLineEdit::returnPressed, this, [this]() {
        loadGitRepo(m_gitRepoPath->text().trimmed());
    });
    connect(m_gitLimitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::updateGitLimitEnabled);
    connect(m_gitLimitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_gitBranchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_gitFromDate, &QDateEdit::dateChanged,
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_gitToDate, &QDateEdit::dateChanged,
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_gitMaxCommits, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MermaidDialog::schedulePreviewUpdate);
    connect(m_gitNoLimit, &QCheckBox::toggled,
            this, &MermaidDialog::schedulePreviewUpdate);

    updateGitLimitEnabled();
    return panel;
}
void MermaidDialog::loadGitRepo(const QString &path)
{
    QStringList branches;
    QString current;
    QDateTime first, last;
    QString error;
    if (!GitGraphBuilder::repoInfo(path, &branches, &current, &first, &last, &error)) {
        m_gitRepo.clear();
        m_gitStatus->setText(error);
        m_gitStatus->setStyleSheet(QStringLiteral("color: #d32f2f;"));
        schedulePreviewUpdate();
        return;
    }

    m_gitRepo = path;
    m_gitRepoPath->setText(path);
    QString status;
    if (first.isValid() && last.isValid())
        status = tr("Commits from %1 to %2")
                     .arg(first.date().toString(Qt::ISODate),
                          last.date().toString(Qt::ISODate));
    else
        status = tr("Repository has no commits.");
    m_gitStatus->setText(status);
    m_gitStatus->setStyleSheet(QStringLiteral("color: palette(mid);"));

    m_gitBranchCombo->blockSignals(true);
    m_gitBranchCombo->clear();
    m_gitBranchCombo->addItems(branches);
    if (!current.isEmpty()) {
        int idx = m_gitBranchCombo->findText(current);
        if (idx >= 0)
            m_gitBranchCombo->setCurrentIndex(idx);
    }
    m_gitBranchCombo->blockSignals(false);

    if (first.isValid() && last.isValid()) {
        m_gitFromDate->setDateRange(first.date(), last.date());
        m_gitToDate->setDateRange(first.date(), last.date());
        m_gitFromDate->setDate(first.date());
        m_gitToDate->setDate(last.date());
    }

    updateGitLimitEnabled();
    schedulePreviewUpdate();
}
void MermaidDialog::updateGitLimitEnabled()
{
    if (!m_gitLimitCombo || !m_gitBranchCombo || !m_gitFromDate || !m_gitToDate)
        return;
    const auto limit = static_cast<GitGraphBuilder::Options::Limit>(
        m_gitLimitCombo->currentData().toInt());
    const bool branchNeeded =
        limit == GitGraphBuilder::Options::Limit::Branch
        || limit == GitGraphBuilder::Options::Limit::BranchAndDates;
    const bool dateNeeded =
        limit == GitGraphBuilder::Options::Limit::Dates
        || limit == GitGraphBuilder::Options::Limit::BranchAndDates;
    m_gitBranchCombo->setEnabled(branchNeeded);
    m_gitFromDate->setEnabled(dateNeeded);
    m_gitToDate->setEnabled(dateNeeded);
}
