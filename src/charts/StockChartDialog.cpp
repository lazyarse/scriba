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
#include "StockChartDialog.h"
#include "StaticHelpers.h"
#include "io/CsvReader.h"
#include "preview/Preview.h"
#include "preview/JsSnippets.h"
#include "charts/Indicators.h"
#include "ChartSource.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QWebEngineView>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <cmath>
#include <limits>

StockChartDialog::StockChartDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Stock Chart Builder");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &StockChartDialog::updatePreview);

    setupUi();
    populateFromRows({"Date", "Open", "High", "Low", "Close", "Volume"},
                     {
                         {"2026-06-01", "152.4", "154.8", "151.2", "153.9", "48231"},
                         {"2026-06-02", "153.9", "155.1", "152.0", "154.6", "39872"},
                         {"2026-06-03", "154.6", "156.7", "153.8", "156.2", "42105"},
                         {"2026-06-04", "156.2", "156.9", "154.1", "154.7", "36540"},
                         {"2026-06-05", "154.7", "155.3", "152.9", "153.4", "45218"},
                         {"2026-06-08", "153.4", "154.2", "151.5", "152.1", "50773"},
                         {"2026-06-09", "152.1", "153.0", "150.8", "151.6", "44129"},
                         {"2026-06-10", "151.6", "152.4", "149.7", "150.3", "58904"},
                         {"2026-06-11", "150.3", "151.9", "149.9", "151.5", "47230"},
                         {"2026-06-12", "151.5", "152.8", "150.6", "152.2", "40311"},
                         {"2026-06-15", "152.2", "153.5", "151.3", "152.9", "38566"},
                         {"2026-06-16", "152.9", "154.1", "152.0", "153.7", "41987"},
                         {"2026-06-17", "153.7", "155.2", "153.1", "154.8", "45602"},
                         {"2026-06-18", "154.8", "156.0", "154.2", "155.5", "44877"},
                         {"2026-06-19", "155.5", "157.3", "155.0", "156.9", "52144"},
                         {"2026-06-22", "156.9", "158.1", "156.1", "157.6", "49355"},
                         {"2026-06-23", "157.6", "158.8", "156.9", "158.3", "43890"},
                         {"2026-06-24", "158.3", "159.0", "157.2", "157.8", "37721"},
                         {"2026-06-25", "157.8", "158.6", "156.4", "157.1", "40298"},
                         {"2026-06-26", "157.1", "157.9", "155.8", "156.4", "43662"},
                         {"2026-06-29", "156.4", "157.2", "155.0", "155.7", "52014"},
                         {"2026-06-30", "155.7", "156.5", "154.3", "155.1", "56877"},
                         {"2026-07-01", "155.1", "156.0", "153.9", "154.6", "61240"},
                         {"2026-07-02", "154.6", "155.4", "153.2", "153.9", "57563"},
                         {"2026-07-03", "153.9", "154.7", "152.8", "153.5", "50812"},
                     });
    updateVolumeEnabled();
    updatePreview();
}

StockChartDialog::StockChartDialog(const QString &existingSpecJson, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Stock Chart Builder");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &StockChartDialog::updatePreview);

    setupUi();
    prefillFromSpec(existingSpecJson);
    updateVolumeEnabled();
    updatePreview();
}

void StockChartDialog::prefillFromSpec(const QString &specJson)
{
    ChartSource::StockSpecData data;
    ChartSource::StockEngine engine = ChartSource::detectStockEngine(specJson.toUtf8());
    bool parsed = false;
    switch (engine) {
    case ChartSource::StockEngine::ECharts:
        parsed = ChartSource::parseStockSpec(specJson.toUtf8(), data);
        break;
    case ChartSource::StockEngine::Lightweight:
        parsed = ChartSource::parseLightweightSpec(specJson.toUtf8(), data);
        break;
    case ChartSource::StockEngine::KlineCharts:
        parsed = ChartSource::parseKlinechartsSpec(specJson.toUtf8(), data);
        break;
    case ChartSource::StockEngine::TradeX:
        parsed = ChartSource::parseTradexSpec(specJson.toUtf8(), data);
        break;
    case ChartSource::StockEngine::Unknown:
        parsed = ChartSource::parseStockSpec(specJson.toUtf8(), data);
        break;
    }
    if (!parsed)
        return;

    if (engine != ChartSource::StockEngine::Unknown) {
        const int engineIdx = m_engineCombo->findData(static_cast<int>(engine));
        if (engineIdx >= 0)
            m_engineCombo->setCurrentIndex(engineIdx);
        const int typeIdx = m_typeCombo->findData(static_cast<int>(data.chartType));
        if (typeIdx >= 0)
            m_typeCombo->setCurrentIndex(typeIdx);
    }

    QList<QStringList> rows;
    rows.reserve(data.dates.size());
    for (int i = 0; i < data.dates.size(); ++i) {
        const QList<double> &o = data.ohlc[i];
        QStringList row;
        row.append(data.dates[i]);
        row.append(QString::number(o[0]));
        row.append(QString::number(o[3]));
        row.append(QString::number(o[2]));
        row.append(QString::number(o[1]));
        if (data.hasVolume && i < data.volumes.size() && data.volumes[i] != 0.0)
            row.append(QString::number(data.volumes[i]));
        else
            row.append(QString());
        rows.append(row);
    }
    populateFromRows({"Date", "Open", "High", "Low", "Close", "Volume"}, rows);

    m_titleEdit->setText(data.title);
    m_volumeCheck->setChecked(data.volume);
    m_zoomCheck->setChecked(data.zoom);
    m_animateCheck->setChecked(data.animate);
    m_ma5Check->setChecked(data.ma5);
    m_ma10Check->setChecked(data.ma10);
    m_ma20Check->setChecked(data.ma20);
    m_ma50Check->setChecked(data.ma50);
    m_macdCheck->setChecked(data.macd);
    m_rsiCheck->setChecked(data.rsi);
    m_bollCheck->setChecked(data.boll);
    m_kdjCheck->setChecked(data.kdj);
}

void StockChartDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    setupLeftPanel(leftPanel);

    m_preview = createPreviewView(this);

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 680});
    splitter->handle(1)->setEnabled(false);

    mainLayout->addWidget(splitter);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    QPushButton *copyBtn = buttonBox->addButton(tr("Cop&y JSON"), QDialogButtonBox::ActionRole);
    buttonBox->addButton(tr("&Insert"), QDialogButtonBox::AcceptRole);
    stripButtonIcons(buttonBox);
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedSpec());
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void StockChartDialog::setupLeftPanel(QWidget *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel("Data:"));
    QHBoxLayout *dataBtnLayout = new QHBoxLayout();
    QPushButton *pasteCsvBtn = new QPushButton("Paste CSV", panel);
    QPushButton *openCsvBtn = new QPushButton("Open CSV", panel);
    dataBtnLayout->addWidget(pasteCsvBtn);
    dataBtnLayout->addWidget(openCsvBtn);
    dataBtnLayout->addStretch();
    layout->addLayout(dataBtnLayout);
    layout->addWidget(new QLabel(
        "Expected columns: date, open, high, low, close[, volume]. "
        "Column names are matched case-insensitively; if not found, "
        "columns are used positionally.", panel));

    m_table = new QTableWidget(0, 6, panel);
    m_table->setHorizontalHeaderLabels({"Date", "Open", "High", "Low", "Close", "Volume"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    layout->addWidget(m_table);

    QGroupBox *optGroup = new QGroupBox("Options", panel);
    QGridLayout *optLayout = new QGridLayout(optGroup);
    optLayout->addWidget(new QLabel("Title:", optGroup), 0, 0);
    m_titleEdit = new QLineEdit(optGroup);
    optLayout->addWidget(m_titleEdit, 0, 1);
    m_volumeCheck = new QCheckBox("Show volume", optGroup);
    optLayout->addWidget(m_volumeCheck, 1, 0, 1, 2);
    m_zoomCheck = new QCheckBox("Zoom / pan (dataZoom)", optGroup);
    m_zoomCheck->setChecked(true);
    optLayout->addWidget(m_zoomCheck, 2, 0, 1, 2);
    m_animateCheck = new QCheckBox("Animate", optGroup);
    m_animateCheck->setObjectName(QStringLiteral("animateCheck"));
    optLayout->addWidget(m_animateCheck, 3, 0, 1, 2);
    optLayout->addWidget(new QLabel("Moving averages:", optGroup), 4, 0, 1, 2);
    m_ma5Check = new QCheckBox("MA5", optGroup);
    m_ma5Check->setChecked(true);
    m_ma10Check = new QCheckBox("MA10", optGroup);
    m_ma20Check = new QCheckBox("MA20", optGroup);
    m_ma20Check->setChecked(true);
    m_ma50Check = new QCheckBox("MA50", optGroup);
    QHBoxLayout *maLayout = new QHBoxLayout();
    maLayout->addWidget(m_ma5Check);
    maLayout->addWidget(m_ma10Check);
    maLayout->addWidget(m_ma20Check);
    maLayout->addWidget(m_ma50Check);
    maLayout->addStretch();
    optLayout->addLayout(maLayout, 5, 0, 1, 2);

    optLayout->addWidget(new QLabel("Engine:", optGroup), 6, 0);
    m_engineCombo = new QComboBox(optGroup);
    m_engineCombo->setObjectName(QStringLiteral("stockEngineCombo"));
    m_engineCombo->addItem("ECharts",
        static_cast<int>(ChartSource::StockEngine::ECharts));
    m_engineCombo->addItem("Lightweight Charts",
        static_cast<int>(ChartSource::StockEngine::Lightweight));
    m_engineCombo->addItem("KlineCharts",
        static_cast<int>(ChartSource::StockEngine::KlineCharts));
    optLayout->addWidget(m_engineCombo, 6, 1);

    optLayout->addWidget(new QLabel("Chart type:", optGroup), 7, 0);
    m_typeCombo = new QComboBox(optGroup);
    m_typeCombo->setObjectName(QStringLiteral("stockTypeCombo"));
    m_typeCombo->addItem("Candlestick",
        static_cast<int>(ChartSource::StockChartType::Candlestick));
    m_typeCombo->addItem("Bar",
        static_cast<int>(ChartSource::StockChartType::Bar));
    m_typeCombo->addItem("Line",
        static_cast<int>(ChartSource::StockChartType::Line));
    m_typeCombo->addItem("Area",
        static_cast<int>(ChartSource::StockChartType::Area));
    optLayout->addWidget(m_typeCombo, 7, 1);

    m_indicatorGroup = new QGroupBox("Indicators", optGroup);
    m_indicatorGroup->setObjectName(QStringLiteral("stockIndicatorGroup"));
    QHBoxLayout *indLayout = new QHBoxLayout(m_indicatorGroup);
    m_macdCheck = new QCheckBox("MACD", m_indicatorGroup);
    m_rsiCheck = new QCheckBox("RSI", m_indicatorGroup);
    m_bollCheck = new QCheckBox("BOLL", m_indicatorGroup);
    m_kdjCheck = new QCheckBox("KDJ", m_indicatorGroup);
    indLayout->addWidget(m_macdCheck);
    indLayout->addWidget(m_rsiCheck);
    indLayout->addWidget(m_bollCheck);
    indLayout->addWidget(m_kdjCheck);
    indLayout->addStretch();
    optLayout->addWidget(m_indicatorGroup, 8, 0, 1, 2);
    m_indicatorGroup->setEnabled(false);
    optLayout->setColumnStretch(1, 1);
    layout->addWidget(optGroup);

    layout->addStretch();

    connect(pasteCsvBtn, &QPushButton::clicked, this, &StockChartDialog::pasteCsv);
    connect(openCsvBtn, &QPushButton::clicked, this, &StockChartDialog::openCsv);
    connect(m_titleEdit, &QLineEdit::textChanged, this, &StockChartDialog::schedulePreviewUpdate);
    QCheckBox *checks[] = {m_volumeCheck, m_zoomCheck, m_animateCheck, m_ma5Check, m_ma10Check, m_ma20Check, m_ma50Check};
    for (QCheckBox *check : checks)
        connect(check, &QCheckBox::toggled, this, &StockChartDialog::schedulePreviewUpdate);
    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, &StockChartDialog::schedulePreviewUpdate);
    QCheckBox *indChecks[] = {m_macdCheck, m_rsiCheck, m_bollCheck, m_kdjCheck};
    for (QCheckBox *check : indChecks)
        connect(check, &QCheckBox::toggled, this, &StockChartDialog::schedulePreviewUpdate);
    connect(m_engineCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const auto engine = static_cast<ChartSource::StockEngine>(
            m_engineCombo->currentData().toInt());
        const bool nonEcharts = engine != ChartSource::StockEngine::ECharts;
        m_indicatorGroup->setEnabled(nonEcharts);
        if (!nonEcharts) {
            m_macdCheck->setChecked(false);
            m_rsiCheck->setChecked(false);
            m_bollCheck->setChecked(false);
            m_kdjCheck->setChecked(false);
        }
        schedulePreviewUpdate();
    });
}

void StockChartDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void StockChartDialog::updatePreview()
{
    const auto engine = static_cast<ChartSource::StockEngine>(
        m_engineCombo->currentData().toInt());
    const QString engineKey = engineName(engine);
    QString spec;
    if (engine == ChartSource::StockEngine::ECharts) {
        QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
        spec = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    } else {
        spec = QString::fromUtf8(
            QJsonDocument(buildPayload()).toJson(QJsonDocument::Compact));
    }

    QString baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    m_preview->setHtml(previewPageHtml(spec, engineKey), QUrl(baseUrl));
}

QString StockChartDialog::previewPageHtml(const QString &spec, const QString &engine)
{
    if (engine == QLatin1String("echarts"))
        return QString(
            "<!DOCTYPE html>"
            "<html><head>"
            "<meta charset=\"utf-8\">"
            "<style>"
            "html,body{margin:0;height:100%;font-family:sans-serif;}"
            "body{display:flex;align-items:flex-start;}"
            "#vis{width:100%;height:90vh;}"
            ".error{color:#d32f2f;padding:16px;font-size:14px;}"
            "</style>"
            "<script src=\"qrc:///echarts.min.js\"></script>"
            "</head><body>"
            "<div id=\"vis\"></div>"
            "<script>"
            "try{"
            "var spec=%1;"
            "var vis=document.getElementById('vis');"
            "var tries=0;"
            "(function go(){"
            "if(vis.clientWidth>0||++tries>%2){"
            "var chart=echarts.init(vis,null,{renderer:'svg'});"
            "try{chart.setOption(spec);}"
            "catch(e){vis.innerHTML='<div class=\"error\">'+e+'</div>';}"
            "}else{setTimeout(go,%3);}"
            "})();"
            "}catch(e){"
            "document.getElementById('vis').innerHTML='<div class=\"error\">'+e+'</div>';"
            "}"
            "</script>"
            "</body></html>"
        ).arg(spec, QString::number(JsTiming::ChartLayoutTries),
              QString::number(JsTiming::ChartLayoutPoll));

    return QString(
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset=\"utf-8\">"
        "<style>"
        "html,body{margin:0;height:100%;font-family:sans-serif;}"
        "body{display:flex;align-items:flex-start;}"
        "#vis{width:100%;height:90vh;}"
        ".error{color:#d32f2f;padding:16px;font-size:14px;}"
        "</style>"
        "</head><body>"
        "<div id=\"vis\"></div>"
        "<script>"
        "%1"
        "</script>"
        "<script>"
        "try{"
        "var payload=%2;"
        "var vis=document.getElementById('vis');"
        "scribaLoadStockEngine('%3').then(function(){"
        "var tries=0;"
        "(function go(){"
        "if(vis.clientWidth>0||++tries>%4){"
        "try{"
        "if('%3'==='lightweight'){scribaRenderLightweight(vis,payload);}"
        "else{scribaRenderKlinecharts(vis,payload);}"
        "}catch(e){vis.innerHTML='<div class=\"error\">'+e+'</div>';}"
        "}else{setTimeout(go,%5);}"
        "})();"
        "});"
        "}catch(e){"
        "document.getElementById('vis').innerHTML='<div class=\"error\">'+e+'</div>';"
        "}"
        "</script>"
        "</body></html>"
    ).arg(stockChartsInitJs, spec, engine,
          QString::number(JsTiming::ChartLayoutTries),
          QString::number(JsTiming::ChartLayoutPoll));
}

QString StockChartDialog::generatedSpec() const
{
    const auto engine = static_cast<ChartSource::StockEngine>(
        m_engineCombo->currentData().toInt());
    if (engine == ChartSource::StockEngine::ECharts) {
        QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
        return QStringLiteral("\n```ec\n%1\n```\n")
            .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    }
    const char *fence = engine == ChartSource::StockEngine::Lightweight
        ? "lc" : "kc";
    return QStringLiteral("\n```%1\nscribaStockChart(\"%2\", %3)\n```\n")
        .arg(QLatin1String(fence), engineName(engine),
             QString::fromUtf8(
                 QJsonDocument(buildPayload()).toJson(QJsonDocument::Compact)));
}

void StockChartDialog::populateFromRows(const QStringList &headers, const QList<QStringList> &rows)
{
    m_ohlc.clear();

    int dateCol = -1, openCol = -1, highCol = -1, lowCol = -1, closeCol = -1, volCol = -1;
    auto lower = [](const QString &s) { return s.trimmed().toLower(); };
    bool named = false;
    for (int c = 0; c < headers.size(); ++c) {
        const QString h = lower(headers[c]);
        if (h == "date" || h == "time" || h == "day" || h == "timestamp") { dateCol = c; named = true; }
        else if (h == "open" || h == "o") { openCol = c; named = true; }
        else if (h == "high" || h == "h") { highCol = c; named = true; }
        else if (h == "low" || h == "l") { lowCol = c; named = true; }
        else if (h == "close" || h == "c") { closeCol = c; named = true; }
        else if (h == "volume" || h == "vol" || h == "v") { volCol = c; named = true; }
    }
    if (!named || closeCol < 0) {
        dateCol = 0; openCol = 1; highCol = 2; lowCol = 3; closeCol = 4; volCol = 5;
    }

    for (const QStringList &row : rows) {
        auto num = [&](int col) -> double {
            if (col < 0 || col >= row.size()) return 0;
            bool ok = false;
            double v = row[col].trimmed().toDouble(&ok);
            return ok ? v : std::numeric_limits<double>::quiet_NaN();
        };
        Ohlc o;
        o.date = (dateCol >= 0 && dateCol < row.size()) ? row[dateCol].trimmed() : QString();
        o.open = num(openCol);
        o.high = num(highCol);
        o.low = num(lowCol);
        o.close = num(closeCol);
        o.hasVolume = volCol >= 0 && volCol < row.size() && !row[volCol].trimmed().isEmpty();
        o.volume = o.hasVolume ? num(volCol) : 0;
        if (!std::isnan(o.open) && !std::isnan(o.high) && !std::isnan(o.low) && !std::isnan(o.close)) {
            o.valid = true;
            if (std::isnan(o.volume)) { o.volume = 0; o.hasVolume = false; }
        }
        m_ohlc.append(o);
    }
    m_ohlc.removeIf([](const Ohlc &o) { return !o.valid; });

    m_table->blockSignals(true);
    m_table->setRowCount(m_ohlc.size());
    for (int r = 0; r < m_ohlc.size(); ++r) {
        const Ohlc &o = m_ohlc[r];
        m_table->setItem(r, 0, new QTableWidgetItem(o.date));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::number(o.open)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::number(o.high)));
        m_table->setItem(r, 3, new QTableWidgetItem(QString::number(o.low)));
        m_table->setItem(r, 4, new QTableWidgetItem(QString::number(o.close)));
        m_table->setItem(r, 5, new QTableWidgetItem(
            o.hasVolume ? QString::number(o.volume) : QString()));
    }
    m_table->blockSignals(false);

    updateVolumeEnabled();
    schedulePreviewUpdate();
}

void StockChartDialog::updateVolumeEnabled()
{
    bool any = false;
    for (const Ohlc &o : m_ohlc)
        if (o.hasVolume) { any = true; break; }
    m_volumeCheck->setEnabled(any);
    if (any)
        m_volumeCheck->setChecked(true);
}

void StockChartDialog::pasteCsv()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Paste OHLC CSV Data");
    dlg.resize(500, 400);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Paste comma-separated data (first row = headers):", &dlg));
    QPlainTextEdit *edit = new QPlainTextEdit(&dlg);
    layout->addWidget(edit);
    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(tr("&OK"));
    box->button(QDialogButtonBox::Cancel)->setText(tr("&Cancel"));
    stripButtonIcons(box);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString text = edit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    CsvData data = CsvReader::readFromString(text, true);
    if (data.headers.isEmpty() && data.rows.isEmpty()) return;

    populateFromRows(data.headers, data.rows);
}

void StockChartDialog::openCsv()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open CSV File", QString(), "CSV Files (*.csv *.tsv *.txt);;All Files (*)");
    if (path.isEmpty()) return;

    CsvData data = CsvReader::readFromFile(path, true);
    if (data.headers.isEmpty() && data.rows.isEmpty())
        return;

    populateFromRows(data.headers, data.rows);
}

QString StockChartDialog::engineName(ChartSource::StockEngine engine)
{
    switch (engine) {
    case ChartSource::StockEngine::ECharts: return QStringLiteral("echarts");
    case ChartSource::StockEngine::Lightweight: return QStringLiteral("lightweight");
    case ChartSource::StockEngine::KlineCharts: return QStringLiteral("klinecharts");
    case ChartSource::StockEngine::TradeX: return QStringLiteral("tradex");
    case ChartSource::StockEngine::Unknown: break;
    }
    return QString();
}

ChartSource::StockEngine StockChartDialog::engineFromName(const QString &name)
{
    const QString key = name.toLower();
    if (key == QLatin1String("echarts")) return ChartSource::StockEngine::ECharts;
    if (key == QLatin1String("lightweight") || key == QLatin1String("lightweight charts"))
        return ChartSource::StockEngine::Lightweight;
    if (key == QLatin1String("klinecharts")) return ChartSource::StockEngine::KlineCharts;
    if (key == QLatin1String("tradex")) return ChartSource::StockEngine::TradeX;
    return ChartSource::StockEngine::Unknown;
}

QString StockChartDialog::typeName(ChartSource::StockChartType type)
{
    switch (type) {
    case ChartSource::StockChartType::Candlestick: return QStringLiteral("candlestick");
    case ChartSource::StockChartType::Bar: return QStringLiteral("bar");
    case ChartSource::StockChartType::Line: return QStringLiteral("line");
    case ChartSource::StockChartType::Area: return QStringLiteral("area");
    }
    return QStringLiteral("candlestick");
}

ChartSource::StockChartType StockChartDialog::typeFromName(const QString &name)
{
    const QString key = name.toLower();
    if (key == QLatin1String("bar")) return ChartSource::StockChartType::Bar;
    if (key == QLatin1String("line")) return ChartSource::StockChartType::Line;
    if (key == QLatin1String("area")) return ChartSource::StockChartType::Area;
    return ChartSource::StockChartType::Candlestick;
}

QString StockChartDialog::buildSpec() const
{
    const auto engine = static_cast<ChartSource::StockEngine>(
        m_engineCombo->currentData().toInt());
    if (engine == ChartSource::StockEngine::ECharts)
        return buildEChartsSpec();
    return QStringLiteral("scribaStockChart(\"%1\", %2)")
        .arg(engineName(engine),
             QString::fromUtf8(
                 QJsonDocument(buildPayload()).toJson(QJsonDocument::Compact)));
}

QJsonObject StockChartDialog::buildPayload() const
{
    QJsonObject payload;
    const QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty())
        payload["title"] = title;
    payload["type"] = typeName(typeFromName(m_typeCombo->currentText()));
    payload["volume"] = m_volumeCheck->isChecked() && m_volumeCheck->isEnabled();
    payload["zoom"] = m_zoomCheck->isChecked();
    payload["animate"] = m_animateCheck->isChecked();

    QJsonArray dates;
    QJsonArray ohlcData;
    QJsonArray volumes;
    QJsonArray closes;
    for (const Ohlc &o : m_ohlc) {
        dates.append(o.date);
        QJsonArray item;
        item.append(o.open);
        item.append(o.close);
        item.append(o.low);
        item.append(o.high);
        ohlcData.append(item);
        volumes.append(o.hasVolume ? QJsonValue(o.volume) : QJsonValue(QJsonValue::Null));
        closes.append(o.close);
    }
    payload["dates"] = dates;
    payload["ohlc"] = ohlcData;
    payload["volumes"] = volumes;

    QList<double> closeValues;
    for (const QJsonValue &v : closes)
        closeValues.append(v.toDouble());

    int maPeriods[] = {5, 10, 20, 50};
    QCheckBox *maChecks[] = {m_ma5Check, m_ma10Check, m_ma20Check, m_ma50Check};
    QJsonArray maArr;
    QJsonObject indicators;
    for (int i = 0; i < 4; ++i) {
        if (!maChecks[i]->isChecked()) continue;
        const int period = maPeriods[i];
        maArr.append(period);
        QJsonArray vals;
        for (double v : Indicators::sma(closeValues, period))
            vals.append(std::isnan(v) ? QJsonValue(QJsonValue::Null) : QJsonValue(v));
        indicators[QStringLiteral("ma%1").arg(period)] = vals;
    }
    payload["ma"] = maArr;

    auto nanToNull = [](const QList<double> &values) {
        QJsonArray arr;
        for (double v : values)
            arr.append(std::isnan(v) ? QJsonValue(QJsonValue::Null) : QJsonValue(v));
        return arr;
    };
    if (m_macdCheck->isChecked()) {
        const Indicators::MacdSeries macd = Indicators::macd(closeValues, 12, 26, 9);
        QJsonObject o;
        o["diff"] = nanToNull(macd.diff);
        o["dea"] = nanToNull(macd.dea);
        o["hist"] = nanToNull(macd.hist);
        indicators["macd"] = o;
    }
    if (m_rsiCheck->isChecked())
        indicators["rsi"] = nanToNull(Indicators::rsi(closeValues, 14));
    if (m_bollCheck->isChecked()) {
        const Indicators::BollSeries boll = Indicators::boll(closeValues, 20, 2.0);
        QJsonObject o;
        o["upper"] = nanToNull(boll.upper);
        o["mid"] = nanToNull(boll.mid);
        o["lower"] = nanToNull(boll.lower);
        indicators["boll"] = o;
    }
    if (m_kdjCheck->isChecked()) {
        QList<double> highs, lows;
        for (const Ohlc &o : m_ohlc) {
            highs.append(o.high);
            lows.append(o.low);
        }
        const Indicators::KdjSeries kdj = Indicators::kdj(highs, lows, closeValues, 9, 3, 3);
        QJsonObject o;
        o["k"] = nanToNull(kdj.k);
        o["d"] = nanToNull(kdj.d);
        o["j"] = nanToNull(kdj.j);
        indicators["kdj"] = o;
    }
    if (!indicators.isEmpty())
        payload["indicators"] = indicators;

    return payload;
}

QString StockChartDialog::buildEChartsSpec() const
{
    if (m_ohlc.isEmpty())
        return QStringLiteral("{}");

    QJsonObject spec;
    if (!m_animateCheck->isChecked())
        spec["animation"] = false;

    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }

    QJsonObject tooltip;
    tooltip["trigger"] = "axis";
    QJsonObject axisPointer;
    axisPointer["type"] = "cross";
    tooltip["axisPointer"] = axisPointer;
    spec["tooltip"] = tooltip;

    bool showVolume = m_volumeCheck->isChecked() && m_volumeCheck->isEnabled();
    bool showZoom = m_zoomCheck->isChecked();

    QJsonArray dates;
    QList<double> closeValues;
    QJsonArray ohlcData;
    QJsonArray closeArr;
    for (const Ohlc &o : m_ohlc) {
        dates.append(o.date);
        closeValues.append(o.close);
        QJsonArray item;
        item.append(o.open);
        item.append(o.close);
        item.append(o.low);
        item.append(o.high);
        ohlcData.append(item);
        closeArr.append(o.close);
    }

    int maPeriods[] = {5, 10, 20, 50};
    QCheckBox *maChecks[] = {m_ma5Check, m_ma10Check, m_ma20Check, m_ma50Check};
    QJsonArray maSeries;
    QJsonArray legendData;
    legendData.append("OHLC");
    for (int i = 0; i < 4; ++i) {
        if (!maChecks[i]->isChecked()) continue;
        QString name = QString("MA%1").arg(maPeriods[i]);
        QJsonArray maArr;
        for (double v : Indicators::sma(closeValues, maPeriods[i]))
            maArr.append(std::isnan(v) ? QJsonValue(QJsonValue::Null) : QJsonValue(v));
        QJsonObject s;
        s["name"] = name;
        s["type"] = "line";
        s["data"] = maArr;
        s["smooth"] = true;
        s["showSymbol"] = false;
        s["lineWidth"] = 1.5;
        maSeries.append(s);
        legendData.append(name);
    }

    const ChartSource::StockChartType chartType = typeFromName(m_typeCombo->currentText());
    QJsonObject mainSeries;
    mainSeries["name"] = "OHLC";
    mainSeries["type"] = chartType == ChartSource::StockChartType::Candlestick
        ? QStringLiteral("candlestick")
        : (chartType == ChartSource::StockChartType::Bar
               ? QStringLiteral("bar") : QStringLiteral("line"));
    mainSeries["data"] = chartType == ChartSource::StockChartType::Candlestick
        ? ohlcData : closeArr;
    if (chartType == ChartSource::StockChartType::Area) {
        QJsonObject areaStyle;
        areaStyle["opacity"] = 0.2;
        mainSeries["areaStyle"] = areaStyle;
    }
    QJsonArray series;
    series.append(mainSeries);
    for (const QJsonValue &v : maSeries)
        series.append(v);

    QJsonArray xAxisArr, yAxisArr, gridArr;

    QJsonObject grid0;
    if (showVolume) {
        grid0["left"] = "5%";
        grid0["right"] = "5%";
        grid0["top"] = "8%";
        grid0["height"] = "58%";
        QJsonObject grid1;
        grid1["left"] = "5%";
        grid1["right"] = "5%";
        grid1["top"] = "75%";
        grid1["height"] = "15%";
        gridArr.append(grid0);
        gridArr.append(grid1);
    } else {
        grid0["left"] = "5%";
        grid0["right"] = "5%";
        grid0["top"] = "8%";
        grid0["bottom"] = "12%";
        gridArr.append(grid0);
    }
    spec["grid"] = gridArr;

    QJsonObject xAxis0;
    xAxis0["type"] = "category";
    xAxis0["data"] = dates;
    xAxis0["boundaryGap"] = false;
    xAxisArr.append(xAxis0);
    if (showVolume) {
        QJsonObject xAxis1;
        xAxis1["type"] = "category";
        xAxis1["gridIndex"] = 1;
        xAxis1["data"] = dates;
        xAxis1["boundaryGap"] = false;
        QJsonObject axisLabel;
        axisLabel["show"] = false;
        xAxis1["axisLabel"] = axisLabel;
        xAxisArr.append(xAxis1);
    }
    spec["xAxis"] = xAxisArr;

    QJsonObject yAxis0;
    yAxis0["type"] = "value";
    yAxis0["scale"] = true;
    yAxisArr.append(yAxis0);
    if (showVolume) {
        QJsonObject yAxis1;
        yAxis1["type"] = "value";
        yAxis1["gridIndex"] = 1;
        yAxis1["scale"] = true;
        yAxis1["splitNumber"] = 2;
        QJsonObject axisLabel;
        axisLabel["show"] = false;
        yAxis1["axisLabel"] = axisLabel;
        QJsonObject splitLine;
        splitLine["show"] = false;
        yAxis1["splitLine"] = splitLine;
        yAxisArr.append(yAxis1);
    }
    spec["yAxis"] = yAxisArr;

    if (showVolume) {
        QJsonObject volume;
        volume["name"] = "Volume";
        volume["type"] = "bar";
        volume["xAxisIndex"] = 1;
        volume["yAxisIndex"] = 1;
        QJsonArray volData;
        for (const Ohlc &o : m_ohlc)
            volData.append(o.hasVolume ? o.volume : QJsonValue(QJsonValue::Null));
        volume["data"] = volData;
        series.append(volume);
        legendData.append("Volume");
    }

    spec["series"] = series;

    QJsonObject legend;
    legend["data"] = legendData;
    spec["legend"] = legend;

    if (showZoom) {
        QJsonArray zoomArr;
        QJsonObject inside;
        inside["type"] = "inside";
        if (showVolume)
            inside["xAxisIndex"] = QJsonArray({0, 1});
        zoomArr.append(inside);
        QJsonObject slider;
        slider["type"] = "slider";
        if (showVolume)
            slider["xAxisIndex"] = QJsonArray({0, 1});
        slider["bottom"] = showVolume ? 5 : 20;
        slider["height"] = 20;
        zoomArr.append(slider);
        spec["dataZoom"] = zoomArr;
    }

    return QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact));
}
