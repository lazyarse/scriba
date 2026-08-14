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
#include "AdvancedChartDialog.h"
#include "StaticHelpers.h"
#include "preview/Preview.h"
#include "ChartDialog.h"
#include "ChartSource.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
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
#include <QIcon>
#include <algorithm>

namespace {

QString comboLabel(ChartSource::EcType type)
{
    switch (type) {
    case ChartSource::EcType::Sankey:
        return QStringLiteral("Sankey Flow");
    case ChartSource::EcType::Boxplot:
        return QStringLiteral("Box Plot");
    case ChartSource::EcType::Parallel:
        return QStringLiteral("Parallel Coordinates");
    case ChartSource::EcType::ThemeRiver:
        return QStringLiteral("Theme River");
    case ChartSource::EcType::Graph:
        return QStringLiteral("Graph (Network)");
    case ChartSource::EcType::Treemap:
        return QStringLiteral("Treemap");
    case ChartSource::EcType::Sunburst:
        return QStringLiteral("Sunburst");
    default:
        return QString();
    }
}

// First-seen order of every node referenced by the sankey links, so the emitted
// `data` array matches what ECharts needs.
QStringList sankeyNodes(const QJsonArray &links)
{
    QStringList nodes;
    for (const QJsonValue &v : links) {
        const QJsonObject l = v.toObject();
        const QString src = l.value("source").toString();
        const QString tgt = l.value("target").toString();
        if (!src.isEmpty() && !nodes.contains(src))
            nodes.append(src);
        if (!tgt.isEmpty() && !nodes.contains(tgt))
            nodes.append(tgt);
    }
    return nodes;
}

struct TreeGroup {
    QString name;
    double value = 0;
    bool hasValue = false;
    std::vector<TreeGroup> children;
};

// Adds a leaf path into the group forest, creating intermediate groups as
// needed (same-name groups at the same depth are merged, preserving order).
void addTreePath(std::vector<TreeGroup> &groups, const QStringList &path, int depth,
                 double value, bool hasValue)
{
    const QString name = path.at(depth);
    auto it = std::find_if(groups.begin(), groups.end(),
        [&](const TreeGroup &g) { return g.name == name; });
    if (it == groups.end()) {
        groups.emplace_back();
        it = groups.end() - 1;
        it->name = name;
    }
    if (depth == path.size() - 1) {
        it->value = value;
        it->hasValue = hasValue || path.size() == 1;
        return;
    }
    addTreePath(it->children, path, depth + 1, value, hasValue);
}

QJsonObject treeToJson(const TreeGroup &group)
{
    QJsonObject o;
    o["name"] = group.name;
    if (group.hasValue)
        o["value"] = group.value;
    if (!group.children.empty()) {
        QJsonArray arr;
        for (const TreeGroup &c : group.children)
            arr.append(treeToJson(c));
        o["children"] = arr;
    }
    return o;
}

} // namespace

AdvancedChartDialog::AdvancedChartDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Advanced Charts");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &AdvancedChartDialog::updatePreview);

    setupUi();
    onTypeChanged();
    updatePreview();
}

AdvancedChartDialog::AdvancedChartDialog(const QString &existingSpecJson, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Advanced Charts");
    resize(1100, 700);

    m_previewTimer = new DebounceTimer(Debounce::DialogPreview, this);
    connect(m_previewTimer, &QTimer::timeout, this, &AdvancedChartDialog::updatePreview);

    setupUi();
    prefillFromSpec(existingSpecJson);
    onTypeChanged();
    updatePreview();
}

void AdvancedChartDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(leftPanel);
    layout->setContentsMargins(12, 12, 12, 12);

    layout->addWidget(new QLabel("Chart Type:"));
    m_typeCombo = new QComboBox(leftPanel);
    const ChartSource::EcType types[] = {
        ChartSource::EcType::Sankey,     ChartSource::EcType::Boxplot,
        ChartSource::EcType::Parallel,   ChartSource::EcType::ThemeRiver,
        ChartSource::EcType::Treemap,    ChartSource::EcType::Sunburst,
        ChartSource::EcType::Graph,
    };
    for (ChartSource::EcType t : types)
        m_typeCombo->addItem(comboLabel(t), static_cast<int>(t));
    layout->addWidget(m_typeCombo);

    m_stack = new QStackedWidget(leftPanel);

    // Sankey — links only; nodes derive from the endpoints.
    m_sankeyTable = buildTable({QStringLiteral("Source"), QStringLiteral("Target"),
                            QStringLiteral("Weight")});
    m_sankeyTable->setRowCount(0);
    const QList<QStringList> sankeyRows = {
        {"Coal", "Transport", "60"},
        {"Coal", "Heating", "40"},
        {"Gas", "Transport", "30"},
    };
    m_sankeyTable->setRowCount(sankeyRows.size());
    for (int r = 0; r < sankeyRows.size(); ++r)
        for (int c = 0; c < sankeyRows[r].size(); ++c)
            m_sankeyTable->setItem(r, c, new QTableWidgetItem(sankeyRows[r][c]));
    m_stack->addWidget(m_sankeyTable);

    // Box Plot — category + five-number summary.
    m_boxTable = buildTable({QStringLiteral("Category"), QStringLiteral("Min"),
                         QStringLiteral("Q1"), QStringLiteral("Median"),
                         QStringLiteral("Q3"), QStringLiteral("Max")});
    const QList<QStringList> boxRows = {
        {"Class A", "54", "65", "71", "82", "93"},
        {"Class B", "61", "71", "77", "86", "90"},
    };
    m_boxTable->setRowCount(boxRows.size());
    for (int r = 0; r < boxRows.size(); ++r)
        for (int c = 0; c < boxRows[r].size(); ++c)
            m_boxTable->setItem(r, c, new QTableWidgetItem(boxRows[r][c]));
    m_stack->addWidget(m_boxTable);

    // Parallel — one column per dimension, one row per line.
    m_parallelTable = buildTable({
        QStringLiteral("Dim 1"), QStringLiteral("Dim 2"),
        QStringLiteral("Dim 3"), QStringLiteral("Dim 4"),
    });
    m_parallelTable->setRowCount(3);
    const QList<QStringList> parallelRows = {
        {"1", "3", "2", "4"},
        {"2", "4", "1", "3"},
        {"3", "2", "4", "1"},
    };
    for (int r = 0; r < parallelRows.size(); ++r)
        for (int c = 0; c < parallelRows[r].size(); ++c)
            m_parallelTable->setItem(r, c, new QTableWidgetItem(parallelRows[r][c]));
    m_stack->addWidget(m_parallelTable);

    // ThemeRiver — date/value/category triples.
    m_themeRiverTable = buildTable({QStringLiteral("Date"), QStringLiteral("Value"),
                                QStringLiteral("Category")});
    const QList<QStringList> themeRows = {
        {"2026-06-01", "5", "Apple"},
        {"2026-06-02", "6", "Apple"},
        {"2026-06-03", "4", "Apple"},
        {"2026-06-01", "3", "Banana"},
        {"2026-06-02", "5", "Banana"},
        {"2026-06-03", "6", "Banana"},
    };
    m_themeRiverTable->setRowCount(themeRows.size());
    for (int r = 0; r < themeRows.size(); ++r)
        for (int c = 0; c < themeRows[r].size(); ++c)
            m_themeRiverTable->setItem(r, c, new QTableWidgetItem(themeRows[r][c]));
    m_stack->addWidget(m_themeRiverTable);

    // Treemap + Sunburst share the flattened-leaf tree editor.
    m_treeTable = buildTable({QStringLiteral("Level 1"), QStringLiteral("Level 2"),
                          QStringLiteral("Value")});
    const QList<QStringList> treeRows = {
        {"nodeA", "nodeAa", "4"},
        {"nodeA", "nodeAb", "6"},
        {"nodeB", "nodeBa", "12"},
        {"nodeB", "nodeBb", "8"},
    };
    m_treeTable->setRowCount(treeRows.size());
    for (int r = 0; r < treeRows.size(); ++r)
        for (int c = 0; c < treeRows[r].size(); ++c)
            m_treeTable->setItem(r, c, new QTableWidgetItem(treeRows[r][c]));
    m_stack->addWidget(m_treeTable); // index 4: treemap
    m_stack->addWidget(m_treeTable); // index 5: sunburst (shared widget)

    // Graph — nodes + links.
    QWidget *graphPanel = new QWidget(leftPanel);
    QVBoxLayout *graphLayout = new QVBoxLayout(graphPanel);
    graphLayout->setContentsMargins(0, 0, 0, 0);
    graphLayout->addWidget(new QLabel("Nodes (Name / Value):", graphPanel));
    m_graphNodes = buildTable({QStringLiteral("Name"), QStringLiteral("Value")});
    m_graphNodes->setRowCount(4);
    for (int r = 0; r < 4; ++r)
        m_graphNodes->setItem(r, 0, new QTableWidgetItem(QStringLiteral("N%1").arg(r + 1)));
    for (int r = 0; r < 4; ++r)
        m_graphNodes->setItem(r, 1, new QTableWidgetItem(QStringLiteral("%1").arg((r + 1) * 10)));
    graphLayout->addWidget(m_graphNodes);
    graphLayout->addWidget(new QLabel("Links (Source / Target / Value):", graphPanel));
    m_graphLinks = buildTable({QStringLiteral("Source"), QStringLiteral("Target"),
                           QStringLiteral("Value")});
    m_graphLinks->setRowCount(4);
    for (int r = 0; r < 4; ++r) {
        m_graphLinks->setItem(r, 0, new QTableWidgetItem(QStringLiteral("N%1").arg(r + 1)));
        m_graphLinks->setItem(r, 1, new QTableWidgetItem(QStringLiteral("N%1").arg(r + 2)));
    }
    graphLayout->addWidget(m_graphLinks);
    m_stack->addWidget(graphPanel); // index 6: graph

    layout->addWidget(m_stack);

    QGroupBox *optGroup = new QGroupBox("Options", leftPanel);
    QGridLayout *optLayout = new QGridLayout(optGroup);
    optLayout->addWidget(new QLabel("Title:", optGroup), 0, 0);
    m_titleEdit = new QLineEdit(optGroup);
    optLayout->addWidget(m_titleEdit, 0, 1);
    m_animateCheck = new QCheckBox("Animate", optGroup);
    m_animateCheck->setObjectName(QStringLiteral("animateCheck"));
    m_animateCheck->setChecked(true);
    optLayout->addWidget(m_animateCheck, 1, 0, 1, 2);
    optLayout->setColumnStretch(1, 1);
    layout->addWidget(optGroup);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addRowBtn = new QPushButton("+Row", leftPanel);
    QPushButton *removeRowBtn = new QPushButton("-Row", leftPanel);
    QPushButton *addColBtn = new QPushButton("+Col", leftPanel);
    QPushButton *removeColBtn = new QPushButton("-Col", leftPanel);
    btnLayout->addWidget(addRowBtn);
    btnLayout->addWidget(removeRowBtn);
    btnLayout->addWidget(addColBtn);
    btnLayout->addWidget(removeColBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    layout->addStretch();

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

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvancedChartDialog::onTypeChanged);
    connect(m_titleEdit, &QLineEdit::textChanged, this, &AdvancedChartDialog::schedulePreviewUpdate);
    connect(m_animateCheck, &QCheckBox::toggled, this, &AdvancedChartDialog::schedulePreviewUpdate);
    connect(addRowBtn, &QPushButton::clicked, this, &AdvancedChartDialog::addRow);
    connect(removeRowBtn, &QPushButton::clicked, this, &AdvancedChartDialog::removeRow);
    connect(addColBtn, &QPushButton::clicked, this, &AdvancedChartDialog::addColumn);
    connect(removeColBtn, &QPushButton::clicked, this, &AdvancedChartDialog::removeColumn);
}

QTableWidget *AdvancedChartDialog::buildTable(const QStringList &headers)
{
    QTableWidget *table = new QTableWidget(0, headers.size(), this);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setDefaultSectionSize(28);
    return table;
}

void AdvancedChartDialog::fillTable(QTableWidget *table, const QList<QStringList> &rows)
{
    table->blockSignals(true);
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size() && c < table->columnCount(); ++c)
            table->setItem(r, c, new QTableWidgetItem(rows[r][c]));
    table->blockSignals(false);
}

void AdvancedChartDialog::prefillFromSpec(const QString &specJson)
{
    const QByteArray json = specJson.toUtf8();
    const ChartSource::EcType type = ChartSource::detectEcType(json);
    int idx = m_typeCombo->findData(static_cast<int>(type));
    if (idx >= 0)
        m_typeCombo->setCurrentIndex(idx);

    switch (type) {
    case ChartSource::EcType::Sankey: {
        ChartSource::SankeySpecData d;
        if (!parseSankeySpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        fillTable(m_sankeyTable, d.links);
        break;
    }
    case ChartSource::EcType::Boxplot: {
        ChartSource::BoxplotSpecData d;
        if (!parseBoxplotSpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        QList<QStringList> rows;
        for (int i = 0; i < d.categories.size(); ++i) {
            QStringList row{d.categories[i]};
            for (double v : d.stats[i])
                row.append(QString::number(v));
            rows.append(row);
        }
        fillTable(m_boxTable, rows);
        break;
    }
    case ChartSource::EcType::Parallel: {
        ChartSource::ParallelSpecData d;
        if (!parseParallelSpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        m_parallelTable->setColumnCount(d.dimensions.size());
        m_parallelTable->setHorizontalHeaderLabels(d.dimensions);
        QList<QStringList> lines;
        for (const auto &line : d.lines) {
            QStringList row;
            for (double v : line)
                row.append(QString::number(v));
            lines.append(row);
        }
        fillTable(m_parallelTable, lines);
        break;
    }
    case ChartSource::EcType::ThemeRiver: {
        ChartSource::ThemeRiverSpecData d;
        if (!parseThemeRiverSpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        fillTable(m_themeRiverTable, d.rows);
        break;
    }
    case ChartSource::EcType::Graph: {
        ChartSource::GraphSpecData d;
        if (!parseGraphSpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        QList<QStringList> nodes;
        for (int i = 0; i < d.nodeNames.size(); ++i) {
            nodes.append({d.nodeNames[i],
                          QString::number(d.nodeValues.value(i, 0.0))});
        }
        fillTable(m_graphNodes, nodes);
        break;
    }
    case ChartSource::EcType::Treemap:
    case ChartSource::EcType::Sunburst: {
        ChartSource::TreeSpecData d;
        if (!parseTreeSpec(json, d))
            break;
        m_titleEdit->setText(d.title);
        m_animateCheck->setChecked(d.animate);
        int depth = 0;
        for (const QStringList &leaf : d.rows)
            depth = std::max(depth, static_cast<int>(leaf.size()) - 1); // level columns (excl. value)
        depth = std::max(depth, 1);
        m_treeTable->setColumnCount(depth + 1);
        QStringList headers;
        for (int k = 1; k <= depth; ++k)
            headers.append(QStringLiteral("Level %1").arg(k));
        headers.append(QStringLiteral("Value"));
        m_treeTable->setHorizontalHeaderLabels(headers);
        // Pad every leaf path into the level columns + value in the final cell.
        QList<QStringList> rows;
        for (const QStringList &leaf : d.rows) {
            QStringList row;
            for (int k = 0; k + 1 < leaf.size(); ++k)
                row.append(leaf[k]);
            while (row.size() < depth)
                row.append(QString());
            row.append(leaf.last());
            rows.append(row);
        }
        fillTable(m_treeTable, rows);
        break;
    }
    default:
        break; // not ours — leave the default panel as-is
    }
}

void AdvancedChartDialog::onTypeChanged()
{
    m_stack->setCurrentIndex(m_typeCombo->currentIndex());
    schedulePreviewUpdate();
}

void AdvancedChartDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void AdvancedChartDialog::updatePreview()
{
    QString spec = buildSpec();
    QJsonDocument doc = QJsonDocument::fromJson(spec.toUtf8());
    QString formatted = doc.toJson(QJsonDocument::Indented);

    QString baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    m_preview->setHtml(ChartDialog::previewPageHtml(formatted), QUrl(baseUrl));
}

QString AdvancedChartDialog::previewPageHtml(const QString &spec)
{
    return ChartDialog::previewPageHtml(spec);
}

QString AdvancedChartDialog::generatedSpec() const
{
    QJsonDocument doc = QJsonDocument::fromJson(buildSpec().toUtf8());
    return QStringLiteral("\n```ec\n%1\n```\n")
        .arg(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

QJsonObject AdvancedChartDialog::baseSpec() const
{
    QJsonObject spec;
    if (!m_animateCheck->isChecked())
        spec["animation"] = false;
    const QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty()) {
        QJsonObject t;
        t["text"] = title;
        spec["title"] = t;
    }
    return spec;
}

QJsonObject AdvancedChartDialog::buildSankey() const
{
    QJsonArray links;
    for (int r = 0; r < m_sankeyTable->rowCount(); ++r) {
        auto cell = [this, r](int c) {
            QTableWidgetItem *it = m_sankeyTable->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        const QString src = cell(0);
        const QString tgt = cell(1);
        if (src.isEmpty() || tgt.isEmpty())
            continue;
        bool ok = false;
        const double value = cell(2).toDouble(&ok);
        QJsonObject link;
        link["source"] = src;
        link["target"] = tgt;
        link["value"] = ok ? value : 0.0;
        links.append(link);
    }
    if (links.isEmpty())
        return {};

    QJsonObject spec = baseSpec();
    QJsonArray nodes;
    for (const QString &name : sankeyNodes(links)) {
        QJsonObject n;
        n["name"] = name;
        nodes.append(n);
    }
    QJsonObject s;
    s["type"] = "sankey";
    s["data"] = nodes;
    s["links"] = links;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject AdvancedChartDialog::buildBoxplot() const
{
    QJsonArray cats, boxes;
    for (int r = 0; r < m_boxTable->rowCount(); ++r) {
        auto cell = [this, r](int c) {
            QTableWidgetItem *it = m_boxTable->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        const QString cat = cell(0);
        if (cat.isEmpty())
            continue;
        QJsonArray box;
        for (int c = 1; c <= 5; ++c) {
            bool num = false;
            const double v = cell(c).toDouble(&num);
            box.append(num ? v : QJsonValue(QJsonValue::Null));
        }
        cats.append(cat);
        boxes.append(box);
    }
    if (cats.isEmpty())
        return {};

    QJsonObject spec = baseSpec();
    QJsonObject xAxis;
    xAxis["type"] = "category";
    xAxis["data"] = cats;
    QJsonObject yAxis;
    yAxis["type"] = "value";
    spec["xAxis"] = xAxis;
    spec["yAxis"] = yAxis;
    QJsonObject s;
    s["type"] = "boxplot";
    s["data"] = boxes;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject AdvancedChartDialog::buildParallel() const
{
    QJsonArray axes;
    const int dims = m_parallelTable->columnCount();
    if (dims < 2)
        return {};
    for (int c = 0; c < dims; ++c) {
        QJsonObject axis;
        axis["dim"] = c;
        QTableWidgetItem *h = m_parallelTable->horizontalHeaderItem(c);
        axis["name"] = (h && !h->text().isEmpty()) ? h->text()
                                                   : QStringLiteral("Dim %1").arg(c + 1);
        axes.append(axis);
    }

    QJsonArray data;
    for (int r = 0; r < m_parallelTable->rowCount(); ++r) {
        QJsonArray line;
        bool any = false;
        for (int c = 0; c < dims; ++c) {
            QTableWidgetItem *it = m_parallelTable->item(r, c);
            bool ok = false;
            const double v = it ? it->text().trimmed().toDouble(&ok) : 0;
            if (ok)
                any = true;
            line.append(ok ? v : QJsonValue(QJsonValue::Null));
        }
        if (any)
            data.append(line);
    }
    if (data.isEmpty())
        return {};

    QJsonObject spec = baseSpec();
    spec["parallelAxis"] = axes;
    spec["parallel"] = QJsonObject();
    QJsonObject s;
    s["type"] = "parallel";
    s["data"] = data;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject AdvancedChartDialog::buildThemeRiver() const
{
    QJsonArray data;
    for (int r = 0; r < m_themeRiverTable->rowCount(); ++r) {
        auto cell = [&, r](int c) {
            QTableWidgetItem *it = m_themeRiverTable->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        const QString date = cell(0);
        const QString cat = cell(2);
        if (date.isEmpty() || cat.isEmpty())
            continue;
        bool ok = false;
        const double value = cell(1).toDouble(&ok);
        data.append(QJsonArray{date, ok ? value : 0.0, cat});
    }
    if (data.isEmpty())
        return {};

    QJsonObject spec = baseSpec();
    spec["singleAxis"] = QJsonObject{{"type", "time"}};
    QJsonObject s;
    s["type"] = "themeRiver";
    s["data"] = data;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject AdvancedChartDialog::buildGraph() const
{
    QJsonArray nodesA;
    for (int r = 0; r < m_graphNodes->rowCount(); ++r) {
        auto cell = [&, r](int c) {
            QTableWidgetItem *it = m_graphNodes->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        const QString name = cell(0);
        if (name.isEmpty())
            continue;
        bool ok = false;
        const double value = cell(1).toDouble(&ok);
        QJsonObject n;
        n["name"] = name;
        if (ok)
            n["value"] = value;
        nodesA.append(n);
    }
    if (nodesA.isEmpty())
        return {};

    QJsonArray links;
    for (int r = 0; r < m_graphLinks->rowCount(); ++r) {
        auto cell = [&, r](int c) {
            QTableWidgetItem *it = m_graphLinks->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        const QString src = cell(0);
        const QString tgt = cell(1);
        if (src.isEmpty() || tgt.isEmpty())
            continue;
        bool ok = false;
        const double value = cell(2).toDouble(&ok);
        QJsonObject l;
        l["source"] = src;
        l["target"] = tgt;
        if (ok)
            l["value"] = value;
        links.append(l);
    }

    QJsonObject spec = baseSpec();
    QJsonObject s;
    s["type"] = "graph";
    if (!links.isEmpty()) {
        s["layout"] = "force";
        s["roam"] = true;
        s["force"] = QJsonObject{{"repulsion", 600}, {"edgeLength", 120}};
    }
    s["data"] = nodesA;
    s["links"] = links;
    spec["series"] = QJsonArray{s};
    return spec;
}

QJsonObject AdvancedChartDialog::buildTree() const
{
    const ChartSource::EcType type =
        static_cast<ChartSource::EcType>(m_typeCombo->currentData().toInt());
    const QString emitType = type == ChartSource::EcType::Sunburst
        ? QStringLiteral("sunburst") : QStringLiteral("treemap");

    std::vector<TreeGroup> groups;
    const int cols = m_treeTable->columnCount();
    for (int r = 0; r < m_treeTable->rowCount(); ++r) {
        QStringList path;
        bool hasValue = false;
        double value = 0;
        for (int c = 0; c < cols; ++c) {
            QTableWidgetItem *it = m_treeTable->item(r, c);
            const QString text = it ? it->text().trimmed() : QString();
            if (c == cols - 1) {
                if (!text.isEmpty()) {
                    hasValue = true;
                    bool ok = false;
                    value = text.toDouble(&ok);
                }
            } else if (!text.isEmpty()) {
                path.append(text);
            }
        }
        if (path.isEmpty())
            continue;
        addTreePath(groups, path, 0, value, hasValue);
    }
    if (groups.empty())
        return {};

    QJsonObject spec = baseSpec();
    QJsonArray data;
    for (const TreeGroup &g : groups)
        data.append(treeToJson(g));
    QJsonObject s;
    s["type"] = emitType;
    s["data"] = data;
    spec["series"] = QJsonArray{s};
    return spec;
}

QString AdvancedChartDialog::buildSpec() const
{
    const ChartSource::EcType type =
        static_cast<ChartSource::EcType>(m_typeCombo->currentData().toInt());
    QJsonObject spec;
    switch (type) {
    case ChartSource::EcType::Sankey:     spec = buildSankey(); break;
    case ChartSource::EcType::Boxplot:    spec = buildBoxplot(); break;
    case ChartSource::EcType::Parallel:   spec = buildParallel(); break;
    case ChartSource::EcType::ThemeRiver: spec = buildThemeRiver(); break;
    case ChartSource::EcType::Graph:      spec = buildGraph(); break;
    case ChartSource::EcType::Treemap:
    case ChartSource::EcType::Sunburst:   spec = buildTree(); break;
    default:                               return QStringLiteral("{}");
    }
    return QString::fromUtf8(QJsonDocument(spec).toJson(QJsonDocument::Compact));
}

QTableWidget *AdvancedChartDialog::currentTable() const
{
    switch (static_cast<ChartSource::EcType>(m_typeCombo->currentData().toInt())) {
    case ChartSource::EcType::Sankey:     return m_sankeyTable;
    case ChartSource::EcType::Boxplot:    return m_boxTable;
    case ChartSource::EcType::Parallel:   return m_parallelTable;
    case ChartSource::EcType::ThemeRiver: return m_themeRiverTable;
    case ChartSource::EcType::Treemap:
    case ChartSource::EcType::Sunburst:   return m_treeTable;
    case ChartSource::EcType::Graph:      return m_graphLinks;
    default:                               return nullptr;
    }
}

void AdvancedChartDialog::addRow()
{
    if (QTableWidget *t = currentTable())
        t->insertRow(t->rowCount());
}

void AdvancedChartDialog::removeRow()
{
    if (QTableWidget *t = currentTable()) {
        if (t->rowCount() > 1)
            t->removeRow(t->rowCount() - 1);
    }
}

void AdvancedChartDialog::addColumn()
{
    QTableWidget *t = currentTable();
    if (!t)
        return;
    if (t == m_parallelTable) {
        const int col = t->columnCount();
        if (col >= 8)
            return;
        t->insertColumn(col);
        t->setHorizontalHeaderItem(col,
            new QTableWidgetItem(QStringLiteral("Dim %1").arg(col + 1)));
        schedulePreviewUpdate();
    } else if (t == m_treeTable) {
        // Insert a level column just before the trailing Value column.
        const int at = t->columnCount() - 1;
        t->insertColumn(at);
        t->setHorizontalHeaderItem(at,
            new QTableWidgetItem(QStringLiteral("Level %1").arg(at)));
        schedulePreviewUpdate();
    }
}

void AdvancedChartDialog::removeColumn()
{
    QTableWidget *t = currentTable();
    if (!t)
        return;
    if (t == m_parallelTable) {
        if (t->columnCount() > 2)
            t->removeColumn(t->columnCount() - 1);
    } else if (t == m_treeTable) {
        // Drop the last non-value level column.
        if (t->columnCount() > 3)
            t->removeColumn(t->columnCount() - 2);
    }
    schedulePreviewUpdate();
}