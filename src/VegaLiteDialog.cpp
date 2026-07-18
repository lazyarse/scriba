#include "VegaLiteDialog.h"
#include "Preview.h"
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
#include <QMessageBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QIcon>

VegaLiteDialog::VegaLiteDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Chart Builder");
    resize(1100, 700);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &VegaLiteDialog::updatePreview);

    setupUi();
    updateFieldComboBoxes();
    if (m_fieldX->count() > 1) m_fieldX->setCurrentIndex(1);
    if (m_fieldY->count() > 2) m_fieldY->setCurrentIndex(2);
    else if (m_fieldY->count() > 1) m_fieldY->setCurrentIndex(1);
    onChartTypeChanged();
    updatePreview();
}

void VegaLiteDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    setupLeftPanel(leftPanel);

    m_preview = new QWebEngineView(this);
    m_preview->setPage(new PreviewPage(m_preview));

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 680});
    splitter->handle(1)->setEnabled(false);

    mainLayout->addWidget(splitter);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    QPushButton *copyBtn = buttonBox->addButton("Copy JSON", QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton("Insert", QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
        QString formatted = doc.toJson(QJsonDocument::Indented);
        QGuiApplication::clipboard()->setText(formatted);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void VegaLiteDialog::setupLeftPanel(QWidget *panel)
{
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel("Chart Type:"));
    m_chartTypeCombo = new QComboBox(panel);
    m_chartTypeCombo->addItems({
        "bar", "line", "point", "area", "rect", "tick", "rule",
        "circle", "square", "text", "trail", "boxplot",
        "errorband", "errorbar", "geoshape"
    });
    layout->addWidget(m_chartTypeCombo);

    layout->addWidget(new QLabel("Data:"));
    QHBoxLayout *dataBtnLayout = new QHBoxLayout();
    QPushButton *pasteCsvBtn = new QPushButton("Paste CSV", panel);
    QPushButton *pasteJsonBtn = new QPushButton("Paste JSON", panel);
    QPushButton *addRowBtn = new QPushButton("+Row", panel);
    QPushButton *removeRowBtn = new QPushButton("-Row", panel);
    QPushButton *addColBtn = new QPushButton("+Col", panel);
    QPushButton *removeColBtn = new QPushButton("-Col", panel);
    dataBtnLayout->addWidget(pasteCsvBtn);
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

    QGroupBox *encGroup = new QGroupBox("Encodings", panel);
    QGridLayout *encLayout = new QGridLayout(encGroup);

    auto makeRow = [&](int row, const QString &label, QComboBox *&field, QComboBox *&type, QWidget *parent) {
        encLayout->addWidget(new QLabel(label, parent), row, 0);
        field = new QComboBox(parent);
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        encLayout->addWidget(field, row, 1);
        type = new QComboBox(parent);
        type->addItems({"nominal", "ordinal", "quantitative", "temporal"});
        encLayout->addWidget(type, row, 2);
    };

    makeRow(0, "X:", m_fieldX, m_typeX, encGroup);
    makeRow(1, "Y:", m_fieldY, m_typeY, encGroup);

    m_colorGroup = new QGroupBox("Color", encGroup);
    QGridLayout *colorLayout = new QGridLayout(m_colorGroup);
    colorLayout->addWidget(new QLabel("Color:", m_colorGroup), 0, 0);
    m_fieldColor = new QComboBox(m_colorGroup);
    m_fieldColor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    colorLayout->addWidget(m_fieldColor, 0, 1);
    m_typeColor = new QComboBox(m_colorGroup);
    m_typeColor->addItems({"nominal", "ordinal", "quantitative", "temporal"});
    colorLayout->addWidget(m_typeColor, 0, 2);
    encLayout->addWidget(m_colorGroup, 2, 0, 1, 3);

    m_sizeGroup = new QGroupBox("Size", encGroup);
    QGridLayout *sizeLayout = new QGridLayout(m_sizeGroup);
    sizeLayout->addWidget(new QLabel("Size:", m_sizeGroup), 0, 0);
    m_fieldSize = new QComboBox(m_sizeGroup);
    m_fieldSize->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sizeLayout->addWidget(m_fieldSize, 0, 1);
    m_typeSize = new QComboBox(m_sizeGroup);
    m_typeSize->addItems({"nominal", "ordinal", "quantitative", "temporal"});
    sizeLayout->addWidget(m_typeSize, 0, 2);
    encLayout->addWidget(m_sizeGroup, 3, 0, 1, 3);

    m_shapeGroup = new QGroupBox("Shape", encGroup);
    QGridLayout *shapeLayout = new QGridLayout(m_shapeGroup);
    shapeLayout->addWidget(new QLabel("Shape:", m_shapeGroup), 0, 0);
    m_fieldShape = new QComboBox(m_shapeGroup);
    m_fieldShape->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    shapeLayout->addWidget(m_fieldShape, 0, 1);
    encLayout->addWidget(m_shapeGroup, 4, 0, 1, 3);

    m_textGroup = new QGroupBox("Text", encGroup);
    QGridLayout *textLayout = new QGridLayout(m_textGroup);
    textLayout->addWidget(new QLabel("Text:", m_textGroup), 0, 0);
    m_fieldText = new QComboBox(m_textGroup);
    m_fieldText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textLayout->addWidget(m_fieldText, 0, 1);
    encLayout->addWidget(m_textGroup, 5, 0, 1, 3);

    m_tooltipCheck = new QCheckBox("Tooltip", encGroup);
    encLayout->addWidget(m_tooltipCheck, 6, 0, 1, 3);

    encLayout->setColumnStretch(1, 1);
    layout->addWidget(encGroup);

    QGroupBox *optGroup = new QGroupBox("Options", panel);
    QGridLayout *optLayout = new QGridLayout(optGroup);
    optLayout->addWidget(new QLabel("Title:", optGroup), 0, 0);
    m_titleEdit = new QLineEdit(optGroup);
    optLayout->addWidget(m_titleEdit, 0, 1);
    m_fillWidthCheck = new QCheckBox("Fill available width", optGroup);
    optLayout->addWidget(m_fillWidthCheck, 1, 0, 1, 2);
    optLayout->setColumnStretch(1, 1);
    layout->addWidget(optGroup);

    layout->addStretch();

    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VegaLiteDialog::onChartTypeChanged);
    connect(m_table, &QTableWidget::itemChanged, this, &VegaLiteDialog::onDataChanged);
    connect(pasteCsvBtn, &QPushButton::clicked, this, &VegaLiteDialog::pasteCsv);
    connect(pasteJsonBtn, &QPushButton::clicked, this, &VegaLiteDialog::pasteJson);
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
    connect(m_titleEdit, &QLineEdit::textChanged, this, &VegaLiteDialog::schedulePreviewUpdate);
    connect(m_fillWidthCheck, &QCheckBox::toggled, this, &VegaLiteDialog::schedulePreviewUpdate);
    connect(m_tooltipCheck, &QCheckBox::toggled, this, &VegaLiteDialog::schedulePreviewUpdate);

    QComboBox *encodingBoxes[] = {m_fieldX, m_typeX, m_fieldY, m_typeY,
                                   m_fieldColor, m_typeColor, m_fieldSize, m_typeSize,
                                   m_fieldShape, m_fieldText};
    for (QComboBox *combo : encodingBoxes)
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &VegaLiteDialog::schedulePreviewUpdate);
}

void VegaLiteDialog::onChartTypeChanged()
{
    updateEncodingVisibility();
    schedulePreviewUpdate();
}

void VegaLiteDialog::onDataChanged()
{
    updateFieldComboBoxes();
    schedulePreviewUpdate();
}

void VegaLiteDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void VegaLiteDialog::updatePreview()
{
    QString spec = buildSpec();
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8());
    QString formatted = doc.toJson(QJsonDocument::Indented);

    QString baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    QString html = QString(
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset=\"utf-8\">"
        "<style>"
        "body{margin:0;display:flex;justify-content:center;align-items:flex-start;padding:16px;font-family:sans-serif;}"
        "#vis{width:100%%;}"
        ".error{color:#d32f2f;padding:16px;font-size:14px;}"
        "</style>"
        "<script src=\"qrc:///vega.min.js\"></script>"
        "<script src=\"qrc:///vega-lite.min.js\"></script>"
        "<script src=\"qrc:///vega-embed.min.js\"></script>"
        "</head><body>"
        "<div id=\"vis\"></div>"
        "<script>"
        "try{"
        "var spec=%1;"
        "vegaEmbed('#vis',spec,{actions:false}).catch(function(e){"
        "document.getElementById('vis').innerHTML='<div class=\"error\">'+e+'</div>';"
        "});"
        "}catch(e){"
        "document.getElementById('vis').innerHTML='<div class=\"error\">'+e+'</div>';"
        "}"
        "</script>"
        "</body></html>"
    ).arg(formatted);

    m_preview->setHtml(html, QUrl(baseUrl));
}

QString VegaLiteDialog::generatedSpec() const
{
    QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
    return doc.toJson(QJsonDocument::Indented);
}

void VegaLiteDialog::pasteCsv()
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
    for (auto *btn : box->buttons())
        btn->setIcon(QIcon());
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString text = edit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    QList<QMap<QString, QString>> rows = parseCsvData(text);
    if (rows.isEmpty()) return;

    QStringList headers = rows.first().keys();

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

void VegaLiteDialog::pasteJson()
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
    for (auto *btn : box->buttons())
        btn->setIcon(QIcon());
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

QList<QMap<QString, QString>> VegaLiteDialog::parseCsvData(const QString &text) const
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

QList<QMap<QString, QString>> VegaLiteDialog::parseJsonData(const QString &text) const
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

void VegaLiteDialog::updateEncodingVisibility()
{
    QString mark = m_chartTypeCombo->currentText();

    bool showSize = false, showShape = false, showText = false;

    if (mark == "point" || mark == "circle" || mark == "square") {
        showSize = true;
        showShape = true;
    } else if (mark == "text") {
        showText = true;
    } else if (mark == "tick" || mark == "trail" || mark == "boxplot") {
        showSize = true;
    }

    m_sizeGroup->setVisible(showSize);
    m_shapeGroup->setVisible(showShape);
    m_textGroup->setVisible(showText);
}

void VegaLiteDialog::updateFieldComboBoxes()
{
    QStringList headers;
    for (int c = 0; c < m_table->columnCount(); ++c) {
        QTableWidgetItem *item = m_table->horizontalHeaderItem(c);
        if (item)
            headers.append(item->text());
    }

    QComboBox *boxes[] = {m_fieldX, m_fieldY, m_fieldColor, m_fieldSize, m_fieldShape, m_fieldText};
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

QString VegaLiteDialog::buildSpec() const
{
    QJsonObject spec;

    QJsonArray values;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QJsonObject row;
        bool hasData = false;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            QTableWidgetItem *headerItem = m_table->horizontalHeaderItem(c);
            if (!headerItem) continue;
            QString key = headerItem->text();
            QTableWidgetItem *cell = m_table->item(r, c);
            QString val = cell ? cell->text() : QString();
            row[key] = val;
            if (!val.isEmpty()) hasData = true;
        }
        if (hasData)
            values.append(row);
    }

    QJsonObject data;
    data["values"] = values;
    spec["data"] = data;

    spec["mark"] = m_chartTypeCombo->currentText();

    QJsonObject encoding;

    auto addEncoding = [&](const QString &name, QComboBox *field, QComboBox *type) {
        QString f = field->currentText();
        if (f.isEmpty()) return;
        QJsonObject enc;
        enc["field"] = f;
        enc["type"] = type->currentText();
        encoding[name] = enc;
    };

    addEncoding("x", m_fieldX, m_typeX);
    addEncoding("y", m_fieldY, m_typeY);
    if (m_colorGroup->isVisible()) {
        addEncoding("color", m_fieldColor, m_typeColor);
    }
    if (m_sizeGroup->isVisible()) {
        addEncoding("size", m_fieldSize, m_typeSize);
    }
    if (m_shapeGroup->isVisible()) {
        QString f = m_fieldShape->currentText();
        if (!f.isEmpty()) {
            QJsonObject enc;
            enc["field"] = f;
            enc["type"] = "nominal";
            encoding["shape"] = enc;
        }
    }
    if (m_textGroup->isVisible()) {
        QString f = m_fieldText->currentText();
        if (!f.isEmpty()) {
            QJsonObject enc;
            enc["field"] = f;
            enc["type"] = "nominal";
            encoding["text"] = enc;
        }
    }
    if (m_tooltipCheck->isChecked()) {
        QJsonArray tooltipArr;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            QTableWidgetItem *header = m_table->horizontalHeaderItem(c);
            if (header && !header->text().isEmpty()) {
                QJsonObject t;
                t["field"] = header->text();
                t["type"] = "nominal";
                tooltipArr.append(t);
            }
        }
        if (!tooltipArr.isEmpty())
            encoding["tooltip"] = tooltipArr;
    }

    if (!encoding.isEmpty())
        spec["encoding"] = encoding;

    if (m_fillWidthCheck->isChecked()) {
        spec["width"] = "container";
        spec["height"] = "container";
    }

    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty())
        spec["title"] = title;

    return QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact));
}
