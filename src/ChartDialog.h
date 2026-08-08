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
class QTableWidget;
class QWebEngineView;
class QGroupBox;
class QLineEdit;
class QCheckBox;
class QTimer;

class ChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChartDialog(QWidget *parent = nullptr);
    // Opens the dialog pre-filled from an existing ` ```ec ` spec block's JSON
    // (as produced by generatedSpec()). When the block isn't a Chart Builder
    // spec the dialog is left in its default empty state.
    explicit ChartDialog(const QString &existingSpecJson, QWidget *parent = nullptr);
    QString generatedSpec() const;
    static QString previewPageHtml(const QString &spec);
    QList<QMap<QString, QString>> parseCsvData(const QString &text) const;
    QList<QMap<QString, QString>> parseJsonData(const QString &text) const;

private slots:
    void onChartTypeChanged();
    void onDataChanged();
    void schedulePreviewUpdate();
    void updatePreview();
    void pasteCsv();
    void openCsv();
    void pasteJson();

private:
    void setupUi();
    void setupLeftPanel(QWidget *panel);
    void updateFieldComboBoxes();
    void updateTypeOptions();
    void populateTableFromCsvData(const struct CsvData &data);
    void prefillFromSpec(const QString &specJson);
    QString buildSpec() const;
    static bool allNumeric(const QStringList &values);

    QComboBox *m_chartTypeCombo;
    QTableWidget *m_table;
    QComboBox *m_fieldX;
    QComboBox *m_fieldY;
    QComboBox *m_fieldZ;
    QCheckBox *m_tooltipCheck;
    QCheckBox *m_animateCheck;
    QCheckBox *m_rippleCheck;
    QCheckBox *m_repeatCheck;
    QLineEdit *m_titleEdit;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};
