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
#include "ChartDialog.h"
#include "StaticHelpers.h"
#include "CsvReader.h"
#include "Preview.h"
#include "ChartSource.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QComboBox>
#include <QTableWidget>
#include <QWebEngineView>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
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
#include <QIcon>

enum class ChartSeries { Bar, Line, Area, Scatter, Pie };

static QString chartSeriesToString(ChartSeries series)
{
    switch (series) {
        case ChartSeries::Bar: return QStringLiteral("bar");
        case ChartSeries::Line: return QStringLiteral("line");
        case ChartSeries::Area: return QStringLiteral("line");
        case ChartSeries::Scatter: return QStringLiteral("scatter");
        case ChartSeries::Pie: return QStringLiteral("pie");
    }
    return {};
}

ChartDialog::ChartDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Chart Builder");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &ChartDialog::updatePreview);

    setupUi();
    updateFieldComboBoxes();
    if (m_fieldX->count() > 1) m_fieldX->setCurrentIndex(1);
    if (m_fieldY->count() > 2) m_fieldY->setCurrentIndex(2);
    else if (m_fieldY->count() > 1) m_fieldY->setCurrentIndex(1);
    onChartTypeChanged();
    updatePreview();
}

ChartDialog::ChartDialog(const QString &existingSpecJson, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Chart Builder");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &ChartDialog::updatePreview);

    setupUi();
    updateFieldComboBoxes();
    if (m_fieldX->count() > 1) m_fieldX->setCurrentIndex(1);
    if (m_fieldY->count() > 2) m_fieldY->setCurrentIndex(2);
    else if (m_fieldY->count() > 1) m_fieldY->setCurrentIndex(1);
    prefillFromSpec(existingSpecJson);
    onChartTypeChanged();
    updatePreview();
}

void ChartDialog::prefillFromSpec(const QString &specJson)
{
    ChartSource::ChartSpecData data;
    if (!ChartSource::parseChartSpec(specJson.toUtf8(), data))
        return;

    const ChartSeries series =
        data.type == QLatin1String("bar") ? ChartSeries::Bar
        : data.type == QLatin1String("line") ? ChartSeries::Line
        : data.type == QLatin1String("area") ? ChartSeries::Area
        : data.type == QLatin1String("scatter") ? ChartSeries::Scatter
        : ChartSeries::Pie;
    int typeIdx = m_chartTypeCombo->findData(static_cast<int>(series));
    if (typeIdx >= 0)
        m_chartTypeCombo->setCurrentIndex(typeIdx);

    m_titleEdit->setText(data.title);
    m_tooltipCheck->setChecked(data.tooltip);
    m_animateCheck->setChecked(data.animate);

    m_table->blockSignals(true);
    m_table->setColumnCount(data.headers.size());
    m_table->setRowCount(data.rows.size());
    m_table->setHorizontalHeaderLabels(data.headers);
    for (int r = 0; r < data.rows.size(); ++r) {
        for (int c = 0; c < data.rows[r].size() && c < data.headers.size(); ++c)
            m_table->setItem(r, c, new QTableWidgetItem(data.rows[r][c]));
    }
    m_table->blockSignals(false);

    updateFieldComboBoxes();
    if (m_fieldX->findText(data.headers.value(0)) >= 0)
        m_fieldX->setCurrentText(data.headers.value(0));
    if (m_fieldY->findText(data.headers.value(1)) >= 0)
        m_fieldY->setCurrentText(data.headers.value(1));
}

void ChartDialog::setupUi()
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

void ChartDialog::setupLeftPanel(QWidget *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel("Chart Type:"));
    m_chartTypeCombo = new QComboBox(panel);
    auto addSeries = [&](const char *name, ChartSeries series) {
        m_chartTypeCombo->addItem(QLatin1String(name), static_cast<int>(series));
    };
    addSeries("Bar", ChartSeries::Bar);
    addSeries("Line", ChartSeries::Line);
    addSeries("Area", ChartSeries::Area);
    addSeries("Scatter", ChartSeries::Scatter);
    addSeries("Pie", ChartSeries::Pie);
    layout->addWidget(m_chartTypeCombo);

    layout->addWidget(new QLabel("Data:"));
    QHBoxLayout *dataBtnLayout = new QHBoxLayout();
    QPushButton *pasteCsvBtn = new QPushButton("Paste CSV", panel);
    QPushButton *openCsvBtn = new QPushButton("Open CSV", panel);
    QPushButton *pasteJsonBtn = new QPushButton("Paste JSON", panel);
    QPushButton *addRowBtn = new QPushButton("+Row", panel);
    QPushButton *removeRowBtn = new QPushButton("-Row", panel);
    QPushButton *addColBtn = new QPushButton("+Col", panel);
    QPushButton *removeColBtn = new QPushButton("-Col", panel);
    dataBtnLayout->addWidget(pasteCsvBtn);
    dataBtnLayout->addWidget(openCsvBtn);
    dataBtnLayout->addWidget(pasteJsonBtn);
    dataBtnLayout->addWidget(addRowBtn);
    dataBtnLayout->addWidget(removeRowBtn);
    dataBtnLayout->addWidget(addColBtn);
    dataBtnLayout->addWidget(removeColBtn);
    dataBtnLayout->addStretch();
    layout->addLayout(dataBtnLayout);

    m_table = new QTableWidget(3, 3, panel);
    m_table->setHorizontalHeaderLabels({"Column 1", "Column 2", "Column 3"});
    m_table->setItem(0, 0, new QTableWidgetItem("A"));
    m_table->setItem(0, 1, new QTableWidgetItem("28"));
    m_table->setItem(1, 0, new QTableWidgetItem("B"));
    m_table->setItem(1, 1, new QTableWidgetItem("55"));
    m_table->setItem(2, 0, new QTableWidgetItem("C"));
    m_table->setItem(2, 1, new QTableWidgetItem("43"));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    layout->addWidget(m_table);

    QGroupBox *encGroup = new QGroupBox("Fields", panel);
    QGridLayout *encLayout = new QGridLayout(encGroup);

    auto makeRow = [&](int row, const QString &label, QComboBox *&field, QWidget *parent) {
        encLayout->addWidget(new QLabel(label, parent), row, 0);
        field = new QComboBox(parent);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        encLayout->addWidget(field, row, 1);
    };

    makeRow(0, "X:", m_fieldX, encGroup);
    makeRow(1, "Y:", m_fieldY, encGroup);

    m_tooltipCheck = new QCheckBox("Tooltip", encGroup);
    encLayout->addWidget(m_tooltipCheck, 2, 0, 1, 2);

    m_animateCheck = new QCheckBox("Animate", encGroup);
    m_animateCheck->setObjectName(QStringLiteral("animateCheck"));
    encLayout->addWidget(m_animateCheck, 3, 0, 1, 2);

    encLayout->setColumnStretch(1, 1);
    layout->addWidget(encGroup);

    QGroupBox *optGroup = new QGroupBox("Options", panel);
    QGridLayout *optLayout = new QGridLayout(optGroup);
    optLayout->addWidget(new QLabel("Title:", optGroup), 0, 0);
    m_titleEdit = new QLineEdit(optGroup);
    optLayout->addWidget(m_titleEdit, 0, 1);
    optLayout->setColumnStretch(1, 1);
    layout->addWidget(optGroup);

    layout->addStretch();

    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartDialog::onChartTypeChanged);
    connect(m_table, &QTableWidget::itemChanged, this, &ChartDialog::onDataChanged);
    connect(pasteCsvBtn, &QPushButton::clicked, this, &ChartDialog::pasteCsv);
    connect(openCsvBtn, &QPushButton::clicked, this, &ChartDialog::openCsv);
    connect(pasteJsonBtn, &QPushButton::clicked, this, &ChartDialog::pasteJson);
    connect(addRowBtn, &QPushButton::clicked, this, [this]() {
        m_table->insertRow(m_table->rowCount());
    });
    connect(removeRowBtn, &QPushButton::clicked, this, [this]() {
        if (m_table->rowCount() > 1)
            m_table->removeRow(m_table->rowCount() - 1);
    });
    connect(addColBtn, &QPushButton::clicked, this, [this]() {
        int col = m_table->columnCount();
        m_table->insertColumn(col);
        m_table->setHorizontalHeaderItem(col, new QTableWidgetItem(QString("Column %1").arg(col + 1)));
        updateFieldComboBoxes();
    });
    connect(removeColBtn, &QPushButton::clicked, this, [this]() {
        if (m_table->columnCount() > 1) {
            m_table->removeColumn(m_table->columnCount() - 1);
            updateFieldComboBoxes();
        }
    });
    connect(m_titleEdit, &QLineEdit::textChanged, this, &ChartDialog::schedulePreviewUpdate);
    connect(m_tooltipCheck, &QCheckBox::toggled, this, &ChartDialog::schedulePreviewUpdate);
    connect(m_animateCheck, &QCheckBox::toggled, this, &ChartDialog::schedulePreviewUpdate);

    QComboBox *fieldBoxes[] = {m_fieldX, m_fieldY};
    for (QComboBox *combo : fieldBoxes)
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ChartDialog::schedulePreviewUpdate);
}

void ChartDialog::onChartTypeChanged()
{
    schedulePreviewUpdate();
}

void ChartDialog::onDataChanged()
{
    updateFieldComboBoxes();
    schedulePreviewUpdate();
}

void ChartDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void ChartDialog::updatePreview()
{
    QString spec = buildSpec();
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8());
    QString formatted = doc.toJson(QJsonDocument::Indented);

    QString baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    m_preview->setHtml(previewPageHtml(formatted), QUrl(baseUrl));
}

QString ChartDialog::previewPageHtml(const QString &spec)
{
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
}

QString ChartDialog::generatedSpec() const
{
    QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
    return QStringLiteral("\n```ec\n%1\n```\n")
        .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

void ChartDialog::populateTableFromCsvData(const CsvData &data)
{
    if (data.rows.isEmpty())
        return;

    m_table->blockSignals(true);
    m_table->setRowCount(data.rows.size());
    m_table->setColumnCount(data.headers.size());
    m_table->setHorizontalHeaderLabels(data.headers);

    for (int r = 0; r < data.rows.size(); ++r) {
        for (int c = 0; c < data.headers.size() && c < data.rows[r].size(); ++c) {
            m_table->setItem(r, c, new QTableWidgetItem(data.rows[r][c]));
        }
    }
    m_table->blockSignals(false);
    updateFieldComboBoxes();
    schedulePreviewUpdate();
}

void ChartDialog::pasteCsv()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Paste CSV Data");
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

    populateTableFromCsvData(data);
}

void ChartDialog::openCsv()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open CSV File", QString(), "CSV Files (*.csv *.tsv *.txt);;All Files (*)");
    if (path.isEmpty()) return;

    CsvData data = CsvReader::readFromFile(path, true);
    if (data.headers.isEmpty() && data.rows.isEmpty())
        return;

    populateTableFromCsvData(data);
}

void ChartDialog::pasteJson()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Paste JSON Data");
    dlg.resize(500, 400);
    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel("Paste a JSON array of objects:", &dlg));
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

    QList<QMap<QString, QString>> rows = parseJsonData(text);
    if (rows.isEmpty()) return;

    QStringList headers;
    for (auto it = rows.first().constBegin(); it != rows.first().constEnd(); ++it)
        headers.append(it.key());

    m_table->blockSignals(true);
    m_table->setRowCount(rows.size());
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);

    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < headers.size(); ++c) {
            m_table->setItem(r, c, new QTableWidgetItem(rows[r].value(headers[c])));
        }
    }
    m_table->blockSignals(false);
    updateFieldComboBoxes();
    schedulePreviewUpdate();
}

QList<QMap<QString, QString>> ChartDialog::parseCsvData(const QString &text) const
{
    QList<QMap<QString, QString>> result;
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) return result;

    QStringList headers;
    for (const QString &h : lines.first().split(','))
        headers.append(h.trimmed());

    for (int i = 1; i < lines.size(); ++i) {
        QStringList fields = lines[i].split(',');
        QMap<QString, QString> row;
        for (int c = 0; c < headers.size(); ++c) {
            row[headers[c]] = c < fields.size() ? fields[c].trimmed() : QString();
        }
        result.append(row);
    }
    return result;
}

QList<QMap<QString, QString>> ChartDialog::parseJsonData(const QString &text) const
{
    QList<QMap<QString, QString>> result;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return result;

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        QMap<QString, QString> row;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            row[it.key()] = it.value().toString();
        }
        result.append(row);
    }
    return result;
}

void ChartDialog::updateFieldComboBoxes()
{
    QStringList headers;
    for (int c = 0; c < m_table->columnCount(); ++c) {
        QTableWidgetItem *item = m_table->horizontalHeaderItem(c);
        if (item)
            headers.append(item->text());
    }

    QComboBox *boxes[] = {m_fieldX, m_fieldY};
    for (QComboBox *combo : boxes) {
        QString current = combo->currentText();
        combo->blockSignals(true);
        combo->clear();
        combo->addItem("");
        combo->addItems(headers);
        int idx = combo->findText(current);
        if (idx >= 0) combo->setCurrentIndex(idx);
        combo->blockSignals(false);
    }
}

bool ChartDialog::allNumeric(const QStringList &values)
{
    bool any = false;
    for (const QString &v : values) {
        if (v.isEmpty()) continue;
        any = true;
        bool ok = false;
        v.toDouble(&ok);
        if (!ok) return false;
    }
    return any;
}

QString ChartDialog::buildSpec() const
{
    QJsonObject spec;
    if (!m_animateCheck->isChecked())
        spec["animation"] = false;

    QString xField = m_fieldX->currentText();
    QString yField = m_fieldY->currentText();
    if (xField.isEmpty() || yField.isEmpty())
        return QStringLiteral("{}");

    QStringList xValues, yValues;
    auto columnIndex = [this](const QString &field) {
        for (int c = 0; c < m_table->columnCount(); ++c) {
            QTableWidgetItem *h = m_table->horizontalHeaderItem(c);
            if (h && h->text() == field)
                return c;
        }
        return -1;
    };
    int xCol = columnIndex(xField);
    int yCol = columnIndex(yField);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *xItem = xCol >= 0 ? m_table->item(r, xCol) : nullptr;
        QTableWidgetItem *yItem = yCol >= 0 ? m_table->item(r, yCol) : nullptr;
        QString xv = xItem ? xItem->text().trimmed() : QString();
        QString yv = yItem ? yItem->text().trimmed() : QString();
        if (xv.isEmpty() && yv.isEmpty()) continue;
        xValues.append(xv);
        yValues.append(yv);
    }
    if (xValues.isEmpty())
        return QStringLiteral("{}");

    ChartSeries series = static_cast<ChartSeries>(m_chartTypeCombo->currentData().toInt());
    QJsonObject tooltip;
    if (m_tooltipCheck->isChecked())
        tooltip["trigger"] = "axis";

    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }
    if (!tooltip.isEmpty())
        spec["tooltip"] = tooltip;

    if (series == ChartSeries::Pie) {
        QJsonArray pieData;
        for (int i = 0; i < xValues.size(); ++i) {
            if (xValues[i].isEmpty()) continue;
            QJsonObject item;
            item["name"] = xValues[i];
            bool ok = false;
            double v = yValues[i].toDouble(&ok);
            item["value"] = ok ? v : 0;
            pieData.append(item);
        }
        QJsonArray seriesArr;
        QJsonObject s;
        s["type"] = chartSeriesToString(series);
        s["data"] = pieData;
        seriesArr.append(s);
        spec["series"] = seriesArr;
        return QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact));
    }

    bool numericX = allNumeric(xValues);
    QJsonObject xAxis;
    xAxis["type"] = numericX ? "value" : "category";
    QJsonObject yAxis;
    yAxis["type"] = "value";

    QJsonArray seriesArr;
    QJsonObject s;
    s["type"] = chartSeriesToString(series);
    if (series == ChartSeries::Area) {
        QJsonObject areaStyle;
        s["areaStyle"] = areaStyle;
    }
    if (numericX) {
        QJsonArray pairs;
        for (int i = 0; i < xValues.size(); ++i) {
            QJsonArray pair;
            bool xok = false, yok = false;
            double xv = xValues[i].toDouble(&xok);
            double yv = yValues[i].toDouble(&yok);
            if (xok && yok) {
                pair.append(xv);
                pair.append(yv);
                pairs.append(pair);
            }
        }
        s["data"] = pairs;
    } else {
        QJsonArray cats, vals;
        for (int i = 0; i < xValues.size(); ++i) {
            cats.append(xValues[i]);
            bool ok = false;
            double v = yValues[i].toDouble(&ok);
            vals.append(ok ? v : QJsonValue(QJsonValue::Null));
        }
        xAxis["data"] = cats;
        s["data"] = vals;
    }
    seriesArr.append(s);

    spec["xAxis"] = xAxis;
    spec["yAxis"] = yAxis;
    spec["series"] = seriesArr;

    return QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact));
}
