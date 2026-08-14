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

class QComboBox;
class QStackedWidget;
class QTableWidget;
class QWebEngineView;
class QLineEdit;
class QCheckBox;
class QTimer;

// One-panel-per-type chart builder for the charts that don't fit the generic
// Chart Builder's X/Y cartesian model: Sankey flow, Box Plot, Parallel
// coordinates, Theme River and Graph — plus the shared tree editor behind both
// Treemap and Sunburst. Emits a ` ```ec ` ECharts JSON block and can be
// re-opened in the matching panel from an existing block (pencil edit).
// See ChartSource::parse{...}Spec in EChartsParser.{h,cpp}.
class AdvancedChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedChartDialog(QWidget *parent = nullptr);
    // Opens the dialog pre-filled from an existing ` ```ec ` spec. The panel is
    // selected from the series type; uneditable specs leave it on the default
    // Sankey panel in its empty state.
    explicit AdvancedChartDialog(const QString &existingSpecJson, QWidget *parent = nullptr);
    QString generatedSpec() const;
    static QString previewPageHtml(const QString &spec);

private slots:
    void onTypeChanged();
    void schedulePreviewUpdate();
    void updatePreview();
    void addRow();
    void removeRow();
    void addColumn();
    void removeColumn();

private:
    void setupUi();
    QTableWidget *buildTable(const QStringList &headers);
    void setCurrentPanelTable(int columnCount);
    void prefillFromSpec(const QString &specJson);
    QString buildSpec() const;
    QJsonObject baseSpec() const;
    QJsonObject buildSankey() const;
    QJsonObject buildBoxplot() const;
    QJsonObject buildParallel() const;
    QJsonObject buildThemeRiver() const;
    QJsonObject buildGraph() const;
    QJsonObject buildTree() const;
    void fillTable(QTableWidget *table, const QList<QStringList> &rows);
    QTableWidget *currentTable() const;

    QComboBox *m_typeCombo;
    QStackedWidget *m_stack;
    QTableWidget *m_sankeyTable;
    QTableWidget *m_boxTable;
    QTableWidget *m_parallelTable;
    QTableWidget *m_themeRiverTable;
    QTableWidget *m_treeTable;
    QTableWidget *m_graphNodes;
    QTableWidget *m_graphLinks;
    QCheckBox *m_animateCheck;
    QLineEdit *m_titleEdit;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};