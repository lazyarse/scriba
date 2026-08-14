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
#pragma once

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTimer;
class QWebEngineView;

namespace ChartSource { struct MermaidData; }

class MermaidDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ChartType {
        Pie, Flowchart, Sequence, Gantt, Class, ER,
        State, Mindmap, Timeline, Journey, Quadrant, Sankey, Radar, GitGraph
    };
    Q_ENUM(ChartType)

    explicit MermaidDialog(const QString &themeCss, QWidget *parent = nullptr);
    // Opens the dialog pre-filled from an existing ` ```mermaid ` diagram. The
    // diagram is reverse-parsed into the matching panel when possible;
    // otherwise (unknown/class/er/legacy syntax) a pre-filled raw-source panel
    // is shown instead.
    explicit MermaidDialog(const QString &existingDiagram, const QString &themeCss,
                           QWidget *parent = nullptr);
    QString mermaidBlock() const;

    static QString mermaidPreviewHtml(const QString &escaped, const QString &theme,
                                      const QString &bgColor = QString());
    static QString emptyPreviewHtml(const QString &bgColor = QString());

private:
    void setupUi();
    void onChartTypeChanged(int index);
    void schedulePreviewUpdate();
    void updatePreview();
    void prefillFromSource(const QString &diagram);
    void applyPrefill(const ChartSource::MermaidData &data);
    void setChartType(int index);
    bool sourceMode() const;

    QWidget *createPiePanel();
    QWidget *createFlowchartPanel();
    QWidget *createSequencePanel();
    QWidget *createGanttPanel();
    QWidget *createClassPanel();
    QWidget *createERPanel();
    QWidget *createStatePanel();
    QWidget *createMindmapPanel();
    QWidget *createTimelinePanel();
    QWidget *createJourneyPanel();
    QWidget *createQuadrantPanel();
    QWidget *createSankeyPanel();
    QWidget *createRadarPanel();
    QWidget *createGitGraphPanel();

    QString buildPieDiagram() const;
    QString buildFlowchartDiagram() const;
    QString buildSequenceDiagram() const;
    QString buildGanttDiagram() const;
    QString buildClassDiagram() const;
    QString buildERDiagram() const;
    QString buildStateDiagram() const;
    QString buildMindmapDiagram() const;
    QString buildTimelineDiagram() const;
    QString buildJourneyDiagram() const;
    QString buildQuadrantDiagram() const;
    QString buildSankeyDiagram() const;
    QString buildRadarDiagram() const;
    QString buildGitGraphDiagram() const;
    QString buildDiagram() const;

    void addDeleteButton(QTableWidget *table, int column, int row,
                         std::function<void()> onDelete = nullptr);
    void populateComboColumns(QTableWidget *table, const QList<int> &columns,
                              const QStringList &items);
    void csvImportForChart(QTableWidget *table, const QStringList &chartFields,
                            const QList<int> &columnIndices);

    // Shared widgets
    QComboBox *m_chartTypeCombo;
    QStackedWidget *m_panels;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
    QCheckBox *m_widthCheck;
    QSpinBox *m_widthSpin;
    QDialogButtonBox *m_buttonBox;
    QString m_themeCss;
    QString m_mermaidTheme;
    QString m_bgColor;
    QColor m_iconColor;

    // Pie
    QLineEdit *m_pieTitle;
    QTableWidget *m_pieTable;

    // Flowchart
    QComboBox *m_fcDirection;
    QTableWidget *m_fcNodeTable;
    QTableWidget *m_fcEdgeTable;
    void refreshEdgeNodeCombos();

    // Sequence
    QTableWidget *m_seqParticipantTable;
    QTableWidget *m_seqMessageTable;
    void refreshMessageCombos();

    // Gantt
    QLineEdit *m_ganttTitle;
    QComboBox *m_ganttDateFormat;
    QCheckBox *m_ganttWeekend;
    QTableWidget *m_ganttTaskTable;

    // Class
    struct ClassData {
        QList<QMap<QString, QString>> fields;
        QList<QMap<QString, QString>> methods;
    };
    QTableWidget *m_classTable;
    QTableWidget *m_classFieldTable;
    QTableWidget *m_classMethodTable;
    QTableWidget *m_classRelationTable;
    QMap<int, ClassData> m_classData;
    int m_lastClassRow = -1;
    void saveCurrentClassData();
    void loadClassData(int classRow);
    void refreshClassRelCombos();

    // ER
    QTableWidget *m_erEntityTable;
    QTableWidget *m_erAttributeTable;
    QTableWidget *m_erRelationTable;
    QMap<int, QList<QMap<QString, QString>>> m_erEntityAttrs;
    int m_lastEntityRow = -1;
    void saveCurrentERAttrs();
    void loadERAttrs(int entityRow);
    void refreshERRelCombos();

    // State
    QTableWidget *m_stateTable;
    QTableWidget *m_stateTransitionTable;
    void refreshStateCombos();

    // Mindmap
    QTreeWidget *m_mindmapTree;

    // Timeline
    QLineEdit *m_timelineTitle;
    QTableWidget *m_timelineTable;

    // Journey
    QLineEdit *m_journeyTitle;
    QTableWidget *m_journeyTable;

    // Quadrant
    QLineEdit *m_quadTitle;
    QLineEdit *m_quadXLeft, *m_quadXRight;
    QLineEdit *m_quadYBottom, *m_quadYTop;
    QLineEdit *m_quadQ1, *m_quadQ2, *m_quadQ3, *m_quadQ4;
    QTableWidget *m_quadTable;

    // Sankey
    QTableWidget *m_sankeyTable;

    // Radar
    QLineEdit *m_radarTitle;
    QTableWidget *m_radarAxisTable;
    QTableWidget *m_radarCurveTable;
    QCheckBox *m_radarShowLegend;
    QSpinBox *m_radarMin;
    QSpinBox *m_radarMax;
    QComboBox *m_radarGraticule;
    QSpinBox *m_radarTicks;

    // Git Graph
    QLineEdit *m_gitRepoPath = nullptr;
    QComboBox *m_gitBranchCombo = nullptr;
    QComboBox *m_gitLimitCombo = nullptr;
    QDateEdit *m_gitFromDate = nullptr;
    QDateEdit *m_gitToDate = nullptr;
    QSpinBox *m_gitMaxCommits = nullptr;
    QCheckBox *m_gitNoLimit = nullptr;
    QLabel *m_gitStatus = nullptr;
    QString m_gitRepo; // validated repository path
    void loadGitRepo(const QString &path);
    void updateGitLimitEnabled();

    // Raw-source fallback panel (index kSourcePanelIndex). Active when an
    // existing diagram could not be reverse-parsed into a structured panel.
    QPlainTextEdit *m_sourceEdit = nullptr;
};
