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

class VegaLiteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VegaLiteDialog(QWidget *parent = nullptr);
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
    void updateEncodingVisibility();
    void updateFieldComboBoxes();
    void populateTableFromCsvData(const struct CsvData &data);
    QString buildSpec() const;

    QComboBox *m_chartTypeCombo;
    QTableWidget *m_table;
    QComboBox *m_fieldX, *m_typeX;
    QComboBox *m_fieldY, *m_typeY;
    QComboBox *m_fieldColor, *m_typeColor;
    QComboBox *m_fieldSize, *m_typeSize;
    QComboBox *m_fieldShape;
    QComboBox *m_fieldText;
    QCheckBox *m_tooltipCheck;
    QLineEdit *m_titleEdit;
    QCheckBox *m_fillWidthCheck;
    QWebEngineView *m_preview;
    QGroupBox *m_colorGroup;
    QGroupBox *m_sizeGroup;
    QGroupBox *m_shapeGroup;
    QGroupBox *m_textGroup;
    QTimer *m_previewTimer;
};

