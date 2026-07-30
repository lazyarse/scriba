#pragma once

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTimer;
class QWebEngineView;

class MermaidDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ChartType {
        Pie, Flowchart, Sequence, Gantt, Class, ER,
        State, Mindmap, Timeline, Journey, Quadrant, Sankey
    };
    Q_ENUM(ChartType)

    explicit MermaidDialog(const QString &themeCss, QWidget *parent = nullptr);
    QString mermaidBlock() const;

    static QString mermaidPreviewHtml(const QString &escaped, const QString &theme,
                                      const QString &bgColor = QString());

private:
    void setupUi();
    void onChartTypeChanged(int index);
    void schedulePreviewUpdate();
    void updatePreview();

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
    QString buildDiagram() const;

    void addDeleteButton(QTableWidget *table, int column, int row,
                         std::function<void()> onDelete = nullptr);
    void populateComboColumns(QTableWidget *table, const QList<int> &columns,
                              const QStringList &items);

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
};
