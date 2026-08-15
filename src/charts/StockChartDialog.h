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

#include "charts/EChartsParser.h"

class QTableWidget;
class QWebEngineView;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QTimer;

class StockChartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StockChartDialog(QWidget *parent = nullptr);
    // Opens the dialog pre-filled from an existing stock spec: an ECharts
    // ` ```ec ` candlestick spec or a `scribaStockChart("engine", ...)`
    // line from an ` ```lc ` / ` ```kc ` / ` ```tx ` fence. Non-stock specs
    // leave it empty.
    explicit StockChartDialog(const QString &existingSpecJson, QWidget *parent = nullptr);
    QString generatedSpec() const;
    static QString previewPageHtml(const QString &spec, const QString &engine);

private slots:
    void schedulePreviewUpdate();
    void updatePreview();
    void pasteCsv();
    void openCsv();

private:
    void setupUi();
    void setupLeftPanel(QWidget *panel);
    void populateFromRows(const QStringList &headers, const QList<QStringList> &rows);
    void updateVolumeEnabled();
    void prefillFromSpec(const QString &specJson);
    QString buildSpec() const;
    QJsonObject buildPayload() const;
    QString buildEChartsSpec() const;
    static QString engineName(ChartSource::StockEngine engine);
    static ChartSource::StockEngine engineFromName(const QString &name);
    static QString typeName(ChartSource::StockChartType type);
    static ChartSource::StockChartType typeFromName(const QString &name);

    struct Ohlc {
        QString date;
        double open = 0;
        double high = 0;
        double low = 0;
        double close = 0;
        double volume = 0;
        bool hasVolume = false;
        bool valid = false;
    };

    QTableWidget *m_table;
    QList<Ohlc> m_ohlc;
    QComboBox *m_engineCombo = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QGroupBox *m_indicatorGroup = nullptr;
    QCheckBox *m_volumeCheck;
    QCheckBox *m_zoomCheck;
    QCheckBox *m_animateCheck;
    QCheckBox *m_ma5Check;
    QCheckBox *m_ma10Check;
    QCheckBox *m_ma20Check;
    QCheckBox *m_ma50Check;
    QCheckBox *m_macdCheck = nullptr;
    QCheckBox *m_rsiCheck = nullptr;
    QCheckBox *m_bollCheck = nullptr;
    QCheckBox *m_kdjCheck = nullptr;
    QLineEdit *m_titleEdit;
    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
};
