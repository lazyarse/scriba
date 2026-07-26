#include "MermaidErDialog.h"
#include "Preview.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QClipboard>
#include <QCheckBox>
#include <QSpinBox>
#include <QWebEngineView>

MermaidErDialog::MermaidErDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase(tr("Insert Mermaid ER Diagram"), themeCss, parent)
{
    resize(900, 550);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Entities section
    leftLayout->addWidget(new QLabel(tr("Entities")));
    m_entitiesTable = new QTableWidget;
    m_entitiesTable->setColumnCount(1);
    m_entitiesTable->setHorizontalHeaderLabels({tr("Name")});
    m_entitiesTable->horizontalHeader()->setStretchLastSection(true);
    m_entitiesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_entitiesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_entitiesTable);

    auto *entityBtns = new QHBoxLayout;
    auto *addEntityBtn = new QPushButton(tr("Add"));
    auto *removeEntityBtn = new QPushButton(tr("Remove"));
    entityBtns->addWidget(addEntityBtn);
    entityBtns->addWidget(removeEntityBtn);
    leftLayout->addLayout(entityBtns);

    connect(addEntityBtn, &QPushButton::clicked, this, &MermaidErDialog::addEntity);
    connect(removeEntityBtn, &QPushButton::clicked, this, &MermaidErDialog::removeEntity);

    // Attributes section
    leftLayout->addWidget(new QLabel(tr("Attributes (for selected entity)")));
    m_attributesTable = new QTableWidget;
    m_attributesTable->setColumnCount(3);
    m_attributesTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Key")});
    m_attributesTable->horizontalHeader()->setStretchLastSection(true);
    m_attributesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_attributesTable);

    auto *attrBtns = new QHBoxLayout;
    auto *addAttrBtn = new QPushButton(tr("Add"));
    auto *removeAttrBtn = new QPushButton(tr("Remove"));
    attrBtns->addWidget(addAttrBtn);
    attrBtns->addWidget(removeAttrBtn);
    leftLayout->addLayout(attrBtns);

    connect(addAttrBtn, &QPushButton::clicked, this, &MermaidErDialog::addAttribute);
    connect(removeAttrBtn, &QPushButton::clicked, this, &MermaidErDialog::removeAttribute);

    // Relations section
    leftLayout->addWidget(new QLabel(tr("Relations")));
    m_relationsTable = new QTableWidget;
    m_relationsTable->setColumnCount(4);
    m_relationsTable->setHorizontalHeaderLabels({tr("From"), tr("To"), tr("Type"), tr("Label")});
    m_relationsTable->horizontalHeader()->setStretchLastSection(true);
    m_relationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_relationsTable);

    auto *relBtns = new QHBoxLayout;
    auto *addRelBtn = new QPushButton(tr("Add"));
    auto *removeRelBtn = new QPushButton(tr("Remove"));
    relBtns->addWidget(addRelBtn);
    relBtns->addWidget(removeRelBtn);
    leftLayout->addLayout(relBtns);

    connect(addRelBtn, &QPushButton::clicked, this, &MermaidErDialog::addRelation);
    connect(removeRelBtn, &QPushButton::clicked, this, &MermaidErDialog::removeRelation);

    connect(m_entitiesTable, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int) { Q_UNUSED(row) updateAttributeTable(); });
    connect(m_entitiesTable, &QTableWidget::cellChanged,
            this, [this]() { refreshRelationCombos(); schedulePreviewUpdate(); });

    // Right panel
    auto *rightWidget = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto *widthRow = new QHBoxLayout;
    m_widthCheck = new QCheckBox(tr("Set max width:"), rightWidget);
    m_widthSpin = new QSpinBox(rightWidget);
    m_widthSpin->setRange(100, 2000);
    m_widthSpin->setValue(500);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setEnabled(false);
    connect(m_widthCheck, &QCheckBox::toggled, m_widthSpin, &QWidget::setEnabled);
    widthRow->addWidget(m_widthCheck);
    widthRow->addWidget(m_widthSpin);
    widthRow->addStretch();
    rightLayout->addLayout(widthRow);
    m_preview = new QWebEngineView(rightWidget);
    m_preview->setPage(new PreviewPage(m_preview));
    rightLayout->addWidget(m_preview);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    QPushButton *copyBtn = buttonBox->addButton(tr("&Copy"), QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton(tr("&Insert"), QDialogButtonBox::AcceptRole);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedDiagram());
    });
    mainLayout->addWidget(buttonBox);

    setupDefaultData();
    updatePreview();
    schedulePreviewUpdate();
}

QString MermaidErDialog::buildDiagram() const
{
    QString out = "erDiagram\n";

    // Relations
    for (int r = 0; r < m_relationsTable->rowCount(); ++r) {
        auto *fromCombo = qobject_cast<QComboBox*>(m_relationsTable->cellWidget(r, 0));
        auto *toCombo = qobject_cast<QComboBox*>(m_relationsTable->cellWidget(r, 1));
        auto *typeCombo = qobject_cast<QComboBox*>(m_relationsTable->cellWidget(r, 2));
        auto *labelItem = m_relationsTable->item(r, 3);
        if (!fromCombo || !toCombo || !typeCombo) continue;
        QString from = fromCombo->currentText();
        QString to = toCombo->currentText();
        QString type = typeCombo->currentText();
        QString label = labelItem ? labelItem->text() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + " " + type + " " + to;
        if (!label.isEmpty())
            out += " : " + label;
        out += "\n";
    }

    // Entity blocks
    for (int e = 0; e < m_entitiesTable->rowCount(); ++e) {
        auto *nameItem = m_entitiesTable->item(e, 0);
        if (!nameItem || nameItem->text().isEmpty()) continue;
        QString entityName = nameItem->text();
        QList<QMap<QString, QString>> attrs = m_entityAttributes.value(e);
        if (attrs.isEmpty()) continue;
        out += "    " + entityName + " {\n";
        for (const auto &attr : attrs) {
            QString attrType = attr.value("type");
            QString attrName = attr.value("name");
            QString key = attr.value("key");
            out += "        " + attrType + " " + attrName;
            if (!key.isEmpty())
                out += " " + key;
            out += "\n";
        }
        out += "    }\n";
    }

    return out;
}

void MermaidErDialog::updateAttributeTable()
{
    int currentRow = m_entitiesTable->currentRow();
    if (currentRow == m_lastEntityRow) return;
    saveCurrentAttributes();
    m_lastEntityRow = currentRow;
    loadAttributes(currentRow);
    schedulePreviewUpdate();
}

void MermaidErDialog::refreshRelationCombos()
{
    QStringList names;
    for (int r = 0; r < m_entitiesTable->rowCount(); ++r) {
        auto *item = m_entitiesTable->item(r, 0);
        if (item && !item->text().isEmpty())
            names.append(item->text());
    }

    for (int r = 0; r < m_relationsTable->rowCount(); ++r) {
        for (int col : {0, 1}) {
            auto *combo = qobject_cast<QComboBox*>(m_relationsTable->cellWidget(r, col));
            if (!combo) continue;
            QString prev = combo->currentText();
            combo->blockSignals(true);
            combo->clear();
            combo->addItems(names);
            int idx = combo->findText(prev);
            if (idx >= 0) combo->setCurrentIndex(idx);
            combo->blockSignals(false);
        }
    }
}

void MermaidErDialog::addEntity()
{
    int row = m_entitiesTable->rowCount();
    m_entitiesTable->insertRow(row);
    m_entitiesTable->setItem(row, 0, new QTableWidgetItem(tr("Entity")));
    m_entitiesTable->setCurrentCell(row, 0);
    refreshRelationCombos();
    schedulePreviewUpdate();
}

void MermaidErDialog::removeEntity()
{
    int row = m_entitiesTable->currentRow();
    if (row < 0) return;
    m_entityAttributes.remove(row);
    m_entitiesTable->removeRow(row);

    QMap<int, QList<QMap<QString, QString>>> newMap;
    for (auto it = m_entityAttributes.constBegin(); it != m_entityAttributes.constEnd(); ++it) {
        if (it.key() > row)
            newMap[it.key() - 1] = it.value();
        else if (it.key() < row)
            newMap[it.key()] = it.value();
    }
    m_entityAttributes = newMap;

    if (m_lastEntityRow == row) {
        m_lastEntityRow = -1;
        m_attributesTable->setRowCount(0);
    } else if (m_lastEntityRow > row) {
        m_lastEntityRow--;
    }

    refreshRelationCombos();
    schedulePreviewUpdate();
}

void MermaidErDialog::addAttribute()
{
    saveCurrentAttributes();
    int row = m_attributesTable->rowCount();
    m_attributesTable->insertRow(row);
    m_attributesTable->setItem(row, 0, new QTableWidgetItem);
    m_attributesTable->setItem(row, 1, new QTableWidgetItem);
    auto *keyCombo = new QComboBox;
    keyCombo->addItems({"", "PK", "FK"});
    m_attributesTable->setCellWidget(row, 2, keyCombo);

    int entityRow = m_entitiesTable->currentRow();
    if (entityRow >= 0) {
        QList<QMap<QString, QString>> &attrs = m_entityAttributes[entityRow];
        QMap<QString, QString> newAttr;
        newAttr["name"] = QString();
        newAttr["type"] = QString();
        newAttr["key"] = QString();
        attrs.append(newAttr);
    }
    schedulePreviewUpdate();
}

void MermaidErDialog::removeAttribute()
{
    int row = m_attributesTable->currentRow();
    if (row < 0) return;
    m_attributesTable->removeRow(row);

    int entityRow = m_entitiesTable->currentRow();
    if (entityRow >= 0) {
        auto &attrs = m_entityAttributes[entityRow];
        if (row < attrs.size())
            attrs.removeAt(row);
    }
    schedulePreviewUpdate();
}

void MermaidErDialog::addRelation()
{
    saveCurrentAttributes();

    int row = m_relationsTable->rowCount();
    m_relationsTable->insertRow(row);

    QStringList names;
    for (int r = 0; r < m_entitiesTable->rowCount(); ++r) {
        auto *item = m_entitiesTable->item(r, 0);
        if (item && !item->text().isEmpty())
            names.append(item->text());
    }

    auto *fromCombo = new QComboBox;
    fromCombo->addItems(names);
    m_relationsTable->setCellWidget(row, 0, fromCombo);

    auto *toCombo = new QComboBox;
    toCombo->addItems(names);
    m_relationsTable->setCellWidget(row, 1, toCombo);

    auto *typeCombo = new QComboBox;
    typeCombo->addItems({"||--o{", "||--||", "}|--||", "}|--o{"});
    m_relationsTable->setCellWidget(row, 2, typeCombo);

    m_relationsTable->setItem(row, 3, new QTableWidgetItem);

    schedulePreviewUpdate();
}

void MermaidErDialog::removeRelation()
{
    int row = m_relationsTable->currentRow();
    if (row < 0) return;
    m_relationsTable->removeRow(row);
    schedulePreviewUpdate();
}

void MermaidErDialog::saveCurrentAttributes()
{
    if (m_lastEntityRow < 0) return;
    QList<QMap<QString, QString>> attrs;
    for (int r = 0; r < m_attributesTable->rowCount(); ++r) {
        QMap<QString, QString> attr;
        auto *nameItem = m_attributesTable->item(r, 0);
        auto *typeItem = m_attributesTable->item(r, 1);
        auto *keyCombo = qobject_cast<QComboBox*>(m_attributesTable->cellWidget(r, 2));
        attr["name"] = nameItem ? nameItem->text() : QString();
        attr["type"] = typeItem ? typeItem->text() : QString();
        attr["key"] = keyCombo ? keyCombo->currentText() : QString();
        attrs.append(attr);
    }
    m_entityAttributes[m_lastEntityRow] = attrs;
}

void MermaidErDialog::loadAttributes(int entityRow)
{
    m_attributesTable->blockSignals(true);
    m_attributesTable->setRowCount(0);
    if (entityRow < 0) {
        m_attributesTable->blockSignals(false);
        return;
    }
    QList<QMap<QString, QString>> attrs = m_entityAttributes.value(entityRow);
    for (const auto &attr : attrs) {
        int r = m_attributesTable->rowCount();
        m_attributesTable->insertRow(r);
        m_attributesTable->setItem(r, 0, new QTableWidgetItem(attr.value("name")));
        m_attributesTable->setItem(r, 1, new QTableWidgetItem(attr.value("type")));
        auto *keyCombo = new QComboBox;
        keyCombo->addItems({"", "PK", "FK"});
        int idx = keyCombo->findText(attr.value("key"));
        if (idx >= 0) keyCombo->setCurrentIndex(idx);
        m_attributesTable->setCellWidget(r, 2, keyCombo);
    }
    m_attributesTable->blockSignals(false);
}

void MermaidErDialog::setupDefaultData()
{
    // Entities
    m_entitiesTable->insertRow(0);
    m_entitiesTable->setItem(0, 0, new QTableWidgetItem("USER"));
    m_entitiesTable->insertRow(1);
    m_entitiesTable->setItem(1, 0, new QTableWidgetItem("POST"));

    // USER attributes
    QList<QMap<QString, QString>> userAttrs;
    QMap<QString, QString> a1; a1["name"] = "id"; a1["type"] = "int"; a1["key"] = "PK"; userAttrs.append(a1);
    QMap<QString, QString> a2; a2["name"] = "email"; a2["type"] = "string"; a2["key"] = ""; userAttrs.append(a2);
    QMap<QString, QString> a3; a3["name"] = "name"; a3["type"] = "string"; a3["key"] = ""; userAttrs.append(a3);
    m_entityAttributes[0] = userAttrs;

    // POST attributes
    QList<QMap<QString, QString>> postAttrs;
    QMap<QString, QString> p1; p1["name"] = "id"; p1["type"] = "int"; p1["key"] = "PK"; postAttrs.append(p1);
    QMap<QString, QString> p2; p2["name"] = "title"; p2["type"] = "string"; p2["key"] = ""; postAttrs.append(p2);
    QMap<QString, QString> p3; p3["name"] = "user_id"; p3["type"] = "int"; p3["key"] = "FK"; postAttrs.append(p3);
    m_entityAttributes[1] = postAttrs;

    // Relation
    m_relationsTable->insertRow(0);
    QStringList names = {"USER", "POST"};
    auto *fromCombo = new QComboBox; fromCombo->addItems(names); fromCombo->setCurrentText("USER");
    auto *toCombo = new QComboBox; toCombo->addItems(names); toCombo->setCurrentText("POST");
    auto *typeCombo = new QComboBox; typeCombo->addItems({"||--o{", "||--||", "}|--||", "}|--o{"});
    m_relationsTable->setCellWidget(0, 0, fromCombo);
    m_relationsTable->setCellWidget(0, 1, toCombo);
    m_relationsTable->setCellWidget(0, 2, typeCombo);
    m_relationsTable->setItem(0, 3, new QTableWidgetItem("writes"));

    m_entitiesTable->setCurrentCell(0, 0);
}
