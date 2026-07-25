#include "MermaidFlowchartDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QIcon>
#include <QGroupBox>

struct ArrowInfo {
    QString display;
    QString left;
    QString right;
};

static const ArrowInfo kArrowTypes[] = {
    {"-->",  "--", "-->"},
    {"---",  "--", "---"},
    {"-.->", "-.", ".->"},
    {"==>",  "==", "==>"},
    {"--o",  "--", "--o"},
    {"--x",  "--", "--x"},
};

static const int kArrowCount = sizeof(kArrowTypes) / sizeof(ArrowInfo);

MermaidFlowchartDialog::MermaidFlowchartDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Flowchart", themeCss, parent)
{
    setupUi();
    resize(1000, 650);
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidFlowchartDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Direction:"));
    m_directionCombo = new QComboBox(leftPanel);
    m_directionCombo->addItem("Top-Down (TD)", "TD");
    m_directionCombo->addItem("Left-Right (LR)", "LR");
    m_directionCombo->addItem("Bottom-Top (BT)", "BT");
    m_directionCombo->addItem("Right-Left (RL)", "RL");
    leftLayout->addWidget(m_directionCombo);

    QGroupBox *nodeGroup = new QGroupBox("Nodes", leftPanel);
    QVBoxLayout *nodeLayout = new QVBoxLayout(nodeGroup);
    QHBoxLayout *nodeBtnLayout = new QHBoxLayout();
    QPushButton *addNodeBtn = new QPushButton("+Node", nodeGroup);
    nodeBtnLayout->addWidget(addNodeBtn);
    nodeBtnLayout->addStretch();
    nodeLayout->addLayout(nodeBtnLayout);

    const int nodeDelCol = 3;
    m_nodeTable = new QTableWidget(2, 4, nodeGroup);
    m_nodeTable->setHorizontalHeaderLabels({"ID", "Text", "Shape", "Del"});
    m_nodeTable->setItem(0, 0, new QTableWidgetItem("A"));
    m_nodeTable->setItem(0, 1, new QTableWidgetItem("Start"));
    m_nodeTable->setItem(1, 0, new QTableWidgetItem("B"));
    m_nodeTable->setItem(1, 1, new QTableWidgetItem("Process"));
    m_nodeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_nodeTable->horizontalHeader()->setSectionResizeMode(nodeDelCol, QHeaderView::Fixed);
    m_nodeTable->setColumnWidth(nodeDelCol, 32);
    m_nodeTable->verticalHeader()->setDefaultSectionSize(28);

    QComboBox *shapeCombo0 = new QComboBox(nodeGroup);
    shapeCombo0->addItems({"box", "round", "stadium", "diamond", "hexagon"});
    m_nodeTable->setCellWidget(0, 2, shapeCombo0);
    QComboBox *shapeCombo1 = new QComboBox(nodeGroup);
    shapeCombo1->addItems({"box", "round", "stadium", "diamond", "hexagon"});
    shapeCombo1->setCurrentIndex(1);
    m_nodeTable->setCellWidget(1, 2, shapeCombo1);

    auto addNodeDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", iconColor(), 16), "", m_nodeTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_nodeTable->setCellWidget(row, nodeDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_nodeTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_nodeTable->rowCount() > 1) {
                m_nodeTable->removeRow(row);
                refreshEdgeNodeCombos();
                schedulePreviewUpdate();
            }
        });
    };
    addNodeDeleteButton(0);
    addNodeDeleteButton(1);

    nodeLayout->addWidget(m_nodeTable);
    leftLayout->addWidget(nodeGroup);

    QGroupBox *edgeGroup = new QGroupBox("Edges", leftPanel);
    QVBoxLayout *edgeLayout = new QVBoxLayout(edgeGroup);
    QHBoxLayout *edgeBtnLayout = new QHBoxLayout();
    QPushButton *addEdgeBtn = new QPushButton("+Edge", edgeGroup);
    edgeBtnLayout->addWidget(addEdgeBtn);
    edgeBtnLayout->addStretch();
    edgeLayout->addLayout(edgeBtnLayout);

    const int edgeDelCol = 4;
    m_edgeTable = new QTableWidget(1, 5, edgeGroup);
    m_edgeTable->setHorizontalHeaderLabels({"From", "To", "Label", "Arrow", "Del"});
    m_edgeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_edgeTable->horizontalHeader()->setSectionResizeMode(edgeDelCol, QHeaderView::Fixed);
    m_edgeTable->setColumnWidth(edgeDelCol, 32);
    m_edgeTable->verticalHeader()->setDefaultSectionSize(28);

    QStringList nodeIds = {"A", "B"};
    QComboBox *fromCombo = new QComboBox(edgeGroup);
    fromCombo->addItems(nodeIds);
    m_edgeTable->setCellWidget(0, 0, fromCombo);

    QComboBox *toCombo = new QComboBox(edgeGroup);
    toCombo->addItems(nodeIds);
    toCombo->setCurrentIndex(1);
    m_edgeTable->setCellWidget(0, 1, toCombo);

    m_edgeTable->setItem(0, 2, new QTableWidgetItem(""));

    QComboBox *arrowCombo = new QComboBox(edgeGroup);
    for (int i = 0; i < kArrowCount; ++i)
        arrowCombo->addItem(kArrowTypes[i].display);
    m_edgeTable->setCellWidget(0, 3, arrowCombo);

    auto addEdgeDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", iconColor(), 16), "", m_edgeTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_edgeTable->setCellWidget(row, edgeDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_edgeTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_edgeTable->rowCount() > 1)
                m_edgeTable->removeRow(row);
            schedulePreviewUpdate();
        });
    };
    addEdgeDeleteButton(0);

    edgeLayout->addWidget(m_edgeTable);
    leftLayout->addWidget(edgeGroup);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {500, 500});

    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidDialogBase::schedulePreviewUpdate);
    connect(m_nodeTable, &QTableWidget::itemChanged, this, &MermaidFlowchartDialog::onNodeChanged);
    connect(m_edgeTable, &QTableWidget::itemChanged, this, &MermaidDialogBase::schedulePreviewUpdate);

    auto onComboChange = [this]() { schedulePreviewUpdate(); };
    connect(shapeCombo0, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(shapeCombo1, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(fromCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(toCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);
    connect(arrowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onComboChange);

    connect(addNodeBtn, &QPushButton::clicked, this, [this, addNodeDeleteButton]() {
        int row = m_nodeTable->rowCount();
        m_nodeTable->insertRow(row);
        QString id = QString(QChar('A' + row));
        m_nodeTable->setItem(row, 0, new QTableWidgetItem(id));
        m_nodeTable->setItem(row, 1, new QTableWidgetItem(""));
        QComboBox *shapeCombo = new QComboBox(m_nodeTable);
        shapeCombo->addItems({"box", "round", "stadium", "diamond", "hexagon"});
        connect(shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidDialogBase::schedulePreviewUpdate);
        m_nodeTable->setCellWidget(row, 2, shapeCombo);
        addNodeDeleteButton(row);
        refreshEdgeNodeCombos();
    });
    connect(addEdgeBtn, &QPushButton::clicked, this, [this, addEdgeDeleteButton]() {
        int row = m_edgeTable->rowCount();
        m_edgeTable->insertRow(row);
        m_edgeTable->setItem(row, 2, new QTableWidgetItem(""));
        addEdgeDeleteButton(row);
        refreshEdgeNodeCombos();
    });
}

void MermaidFlowchartDialog::onNodeChanged()
{
    refreshEdgeNodeCombos();
    schedulePreviewUpdate();
}

void MermaidFlowchartDialog::refreshEdgeNodeCombos()
{
    QStringList nodeIds;
    for (int r = 0; r < m_nodeTable->rowCount(); ++r) {
        QTableWidgetItem *item = m_nodeTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            nodeIds.append(item->text().trimmed());
    }

    for (int r = 0; r < m_edgeTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, 1));

        if (fromBox) {
            QString cur = fromBox->currentText();
            fromBox->blockSignals(true);
            fromBox->clear();
            fromBox->addItems(nodeIds);
            int idx = fromBox->findText(cur);
            if (idx >= 0) fromBox->setCurrentIndex(idx);
            fromBox->blockSignals(false);
        }
        if (toBox) {
            QString cur = toBox->currentText();
            toBox->blockSignals(true);
            toBox->clear();
            toBox->addItems(nodeIds);
            int idx = toBox->findText(cur);
            if (idx >= 0) toBox->setCurrentIndex(idx);
            else if (!toBox->currentText().isEmpty()) {}
            toBox->blockSignals(false);
        }

        if (!fromBox || !toBox) {
            QComboBox *newFrom = new QComboBox(m_edgeTable);
            newFrom->addItems(nodeIds);
            m_edgeTable->setCellWidget(r, 0, newFrom);
            connect(newFrom, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialogBase::schedulePreviewUpdate);

            QComboBox *newTo = new QComboBox(m_edgeTable);
            newTo->addItems(nodeIds);
            m_edgeTable->setCellWidget(r, 1, newTo);
            connect(newTo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialogBase::schedulePreviewUpdate);
        }
    }

    int arrowCol = 3;
    for (int r = 0; r < m_edgeTable->rowCount(); ++r) {
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, arrowCol));
        if (!arrowBox) {
            arrowBox = new QComboBox(m_edgeTable);
            for (int i = 0; i < kArrowCount; ++i)
                arrowBox->addItem(kArrowTypes[i].display);
            m_edgeTable->setCellWidget(r, arrowCol, arrowBox);
            connect(arrowBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidDialogBase::schedulePreviewUpdate);
        }
    }
}

QString MermaidFlowchartDialog::shapeToMermaid(const QString &shape, const QString &text)
{
    if (shape == "round")
        return "(" + text + ")";
    if (shape == "stadium")
        return "([" + text + "])";
    if (shape == "diamond")
        return "{" + text + "}";
    if (shape == "hexagon")
        return "{{" + text + "}}";
    return "[" + text + "]";
}

QString MermaidFlowchartDialog::renderEdge(
    const QString &from, const QString &to,
    const QString &label, const QString &arrowType)
{
    const ArrowInfo *info = nullptr;
    for (int i = 0; i < kArrowCount; ++i) {
        if (kArrowTypes[i].display == arrowType) {
            info = &kArrowTypes[i];
            break;
        }
    }
    if (!info)
        return from + "-->" + to;

    if (label.isEmpty())
        return from + info->display + to;

    return from + info->left + " " + label + " " + info->right + to;
}

QString MermaidFlowchartDialog::buildDiagram() const
{
    QString dir = m_directionCombo->currentData().toString();
    QString out = "flowchart " + dir + "\n";

    for (int r = 0; r < m_nodeTable->rowCount(); ++r) {
        QTableWidgetItem *idItem = m_nodeTable->item(r, 0);
        QTableWidgetItem *textItem = m_nodeTable->item(r, 1);
        QComboBox *shapeBox = qobject_cast<QComboBox*>(m_nodeTable->cellWidget(r, 2));

        QString id = idItem ? idItem->text().trimmed() : QString();
        QString text = textItem ? textItem->text().trimmed() : id;
        QString shape = shapeBox ? shapeBox->currentText() : "box";

        if (id.isEmpty()) continue;

        if (text.isEmpty()) text = id;

        out += "    " + id + shapeToMermaid(shape, text) + "\n";
    }

    for (int r = 0; r < m_edgeTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, 1));
        QTableWidgetItem *labelItem = m_edgeTable->item(r, 2);
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_edgeTable->cellWidget(r, 3));

        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString arrow = arrowBox ? arrowBox->currentText() : QString("-->");

        if (from.isEmpty() || to.isEmpty() || from == to) continue;

        out += "    " + renderEdge(from, to, label, arrow) + "\n";
    }

    return out;
}
