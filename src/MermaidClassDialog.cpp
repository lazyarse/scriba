#include "MermaidClassDialog.h"
#include "Preview.h"

#include <QCheckBox>
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
#include <QSpinBox>
#include <QWebEngineView>

MermaidClassDialog::MermaidClassDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase(tr("Insert Mermaid Class Diagram"), themeCss, parent)
{
    resize(900, 550);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Classes section
    leftLayout->addWidget(new QLabel(tr("Classes")));
    m_classesTable = new QTableWidget;
    m_classesTable->setColumnCount(2);
    m_classesTable->setHorizontalHeaderLabels({tr("Name"), tr("Type")});
    m_classesTable->horizontalHeader()->setStretchLastSection(true);
    m_classesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_classesTable);

    auto *classBtns = new QHBoxLayout;
    auto *addClassBtn = new QPushButton(tr("Add"));
    auto *removeClassBtn = new QPushButton(tr("Remove"));
    classBtns->addWidget(addClassBtn);
    classBtns->addWidget(removeClassBtn);
    leftLayout->addLayout(classBtns);

    connect(addClassBtn, &QPushButton::clicked, this, &MermaidClassDialog::addClass);
    connect(removeClassBtn, &QPushButton::clicked, this, &MermaidClassDialog::removeClass);

    // Fields section
    leftLayout->addWidget(new QLabel(tr("Fields (for selected class)")));
    m_fieldsTable = new QTableWidget;
    m_fieldsTable->setColumnCount(4);
    m_fieldsTable->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Visibility"), tr("Static")});
    m_fieldsTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_fieldsTable);

    auto *fieldBtns = new QHBoxLayout;
    auto *addFieldBtn = new QPushButton(tr("Add"));
    auto *removeFieldBtn = new QPushButton(tr("Remove"));
    fieldBtns->addWidget(addFieldBtn);
    fieldBtns->addWidget(removeFieldBtn);
    leftLayout->addLayout(fieldBtns);

    connect(addFieldBtn, &QPushButton::clicked, this, &MermaidClassDialog::addField);
    connect(removeFieldBtn, &QPushButton::clicked, this, &MermaidClassDialog::removeField);

    // Methods section
    leftLayout->addWidget(new QLabel(tr("Methods (for selected class)")));
    m_methodsTable = new QTableWidget;
    m_methodsTable->setColumnCount(4);
    m_methodsTable->setHorizontalHeaderLabels({tr("Name"), tr("Return Type"), tr("Parameters"), tr("Visibility")});
    m_methodsTable->horizontalHeader()->setStretchLastSection(true);
    m_methodsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    leftLayout->addWidget(m_methodsTable);

    auto *methodBtns = new QHBoxLayout;
    auto *addMethodBtn = new QPushButton(tr("Add"));
    auto *removeMethodBtn = new QPushButton(tr("Remove"));
    methodBtns->addWidget(addMethodBtn);
    methodBtns->addWidget(removeMethodBtn);
    leftLayout->addLayout(methodBtns);

    connect(addMethodBtn, &QPushButton::clicked, this, &MermaidClassDialog::addMethod);
    connect(removeMethodBtn, &QPushButton::clicked, this, &MermaidClassDialog::removeMethod);

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

    connect(addRelBtn, &QPushButton::clicked, this, &MermaidClassDialog::addRelation);
    connect(removeRelBtn, &QPushButton::clicked, this, &MermaidClassDialog::removeRelation);

    connect(m_classesTable, &QTableWidget::currentCellChanged,
            this, [this](int row, int, int, int) { Q_UNUSED(row) updateFieldTable(); updateMethodTable(); });
    connect(m_classesTable, &QTableWidget::cellChanged,
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
    QPushButton *copyBtn = buttonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton(tr("Insert"), QDialogButtonBox::AcceptRole);
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

QString MermaidClassDialog::buildDiagram() const
{
    QString out = "classDiagram\n";

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

    // Class blocks
    for (int c = 0; c < m_classesTable->rowCount(); ++c) {
        auto *nameItem = m_classesTable->item(c, 0);
        auto *typeCombo = qobject_cast<QComboBox*>(m_classesTable->cellWidget(c, 1));
        if (!nameItem || nameItem->text().isEmpty()) continue;
        QString className = nameItem->text();
        QString classType = typeCombo ? typeCombo->currentText() : "class";

        ClassData data = m_classData.value(c);

        if (classType == "enumeration") {
            out += "    enum " + className + " {\n";
        } else {
            out += "    class " + className + " {\n";
        }

        for (const auto &field : data.fields) {
            QString vis = field.value("visibility", "+");
            QString type = field.value("type");
            QString name = field.value("name");
            bool isStatic = field.value("static") == "true";
            out += "        " + vis + type + " " + name;
            if (isStatic)
                out += " $";
            out += "\n";
        }
        for (const auto &method : data.methods) {
            QString vis = method.value("visibility", "+");
            QString retType = method.value("returnType");
            QString name = method.value("name");
            QString params = method.value("parameters");
            out += "        " + vis + name + "(" + params + ") " + retType + "\n";
        }
        out += "    }\n";
    }

    return out;
}

void MermaidClassDialog::updateFieldTable()
{
    int currentRow = m_classesTable->currentRow();
    if (currentRow == m_lastClassRow) return;
    saveCurrentClassData();
    m_lastClassRow = currentRow;
    loadClassData(currentRow);
    schedulePreviewUpdate();
}

void MermaidClassDialog::updateMethodTable()
{
    // Handled by loadClassData in updateFieldTable
}

void MermaidClassDialog::refreshRelationCombos()
{
    QStringList names;
    for (int r = 0; r < m_classesTable->rowCount(); ++r) {
        auto *item = m_classesTable->item(r, 0);
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

void MermaidClassDialog::addClass()
{
    int row = m_classesTable->rowCount();
    m_classesTable->insertRow(row);
    m_classesTable->setItem(row, 0, new QTableWidgetItem(tr("MyClass")));
    auto *typeCombo = new QComboBox;
    typeCombo->addItems({"class", "interface", "abstract", "enumeration"});
    m_classesTable->setCellWidget(row, 1, typeCombo);
    m_classesTable->setCurrentCell(row, 0);
    refreshRelationCombos();
    schedulePreviewUpdate();
}

void MermaidClassDialog::removeClass()
{
    int row = m_classesTable->currentRow();
    if (row < 0) return;
    m_classData.remove(row);
    m_classesTable->removeRow(row);

    QMap<int, ClassData> newData;
    for (auto it = m_classData.constBegin(); it != m_classData.constEnd(); ++it) {
        if (it.key() > row)
            newData[it.key() - 1] = it.value();
        else if (it.key() < row)
            newData[it.key()] = it.value();
    }
    m_classData = newData;

    if (m_lastClassRow == row) {
        m_lastClassRow = -1;
        m_fieldsTable->setRowCount(0);
        m_methodsTable->setRowCount(0);
    } else if (m_lastClassRow > row) {
        m_lastClassRow--;
    }

    refreshRelationCombos();
    schedulePreviewUpdate();
}

void MermaidClassDialog::addField()
{
    saveCurrentClassData();
    int row = m_fieldsTable->rowCount();
    m_fieldsTable->insertRow(row);
    m_fieldsTable->setItem(row, 0, new QTableWidgetItem);
    m_fieldsTable->setItem(row, 1, new QTableWidgetItem);
    auto *visCombo = new QComboBox;
    visCombo->addItems({"+", "-", "#", "~"});
    m_fieldsTable->setCellWidget(row, 2, visCombo);
    auto *staticCheck = new QCheckBox;
    staticCheck->setCheckState(Qt::Unchecked);
    m_fieldsTable->setCellWidget(row, 3, staticCheck);

    int classRow = m_classesTable->currentRow();
    if (classRow >= 0) {
        QMap<QString, QString> field;
        field["name"] = QString();
        field["type"] = QString();
        field["visibility"] = "+";
        field["static"] = "false";
        m_classData[classRow].fields.append(field);
    }
    schedulePreviewUpdate();
}

void MermaidClassDialog::removeField()
{
    int row = m_fieldsTable->currentRow();
    if (row < 0) return;
    m_fieldsTable->removeRow(row);
    int classRow = m_classesTable->currentRow();
    if (classRow >= 0) {
        auto &fields = m_classData[classRow].fields;
        if (row < fields.size())
            fields.removeAt(row);
    }
    schedulePreviewUpdate();
}

void MermaidClassDialog::addMethod()
{
    saveCurrentClassData();
    int row = m_methodsTable->rowCount();
    m_methodsTable->insertRow(row);
    m_methodsTable->setItem(row, 0, new QTableWidgetItem);
    m_methodsTable->setItem(row, 1, new QTableWidgetItem);
    m_methodsTable->setItem(row, 2, new QTableWidgetItem);
    auto *visCombo = new QComboBox;
    visCombo->addItems({"+", "-", "#", "~"});
    m_methodsTable->setCellWidget(row, 3, visCombo);

    int classRow = m_classesTable->currentRow();
    if (classRow >= 0) {
        QMap<QString, QString> method;
        method["name"] = QString();
        method["returnType"] = QString();
        method["parameters"] = QString();
        method["visibility"] = "+";
        m_classData[classRow].methods.append(method);
    }
    schedulePreviewUpdate();
}

void MermaidClassDialog::removeMethod()
{
    int row = m_methodsTable->currentRow();
    if (row < 0) return;
    m_methodsTable->removeRow(row);
    int classRow = m_classesTable->currentRow();
    if (classRow >= 0) {
        auto &methods = m_classData[classRow].methods;
        if (row < methods.size())
            methods.removeAt(row);
    }
    schedulePreviewUpdate();
}

void MermaidClassDialog::addRelation()
{
    saveCurrentClassData();
    int row = m_relationsTable->rowCount();
    m_relationsTable->insertRow(row);

    QStringList names;
    for (int r = 0; r < m_classesTable->rowCount(); ++r) {
        auto *item = m_classesTable->item(r, 0);
        if (item && !item->text().isEmpty())
            names.append(item->text());
    }

    auto *fromCombo = new QComboBox; fromCombo->addItems(names);
    auto *toCombo = new QComboBox; toCombo->addItems(names);
    auto *typeCombo = new QComboBox;
    typeCombo->addItems({"-->", "<|--", "..|>", "*--", "o--"});
    m_relationsTable->setCellWidget(row, 0, fromCombo);
    m_relationsTable->setCellWidget(row, 1, toCombo);
    m_relationsTable->setCellWidget(row, 2, typeCombo);
    m_relationsTable->setItem(row, 3, new QTableWidgetItem);
    schedulePreviewUpdate();
}

void MermaidClassDialog::removeRelation()
{
    int row = m_relationsTable->currentRow();
    if (row < 0) return;
    m_relationsTable->removeRow(row);
    schedulePreviewUpdate();
}

void MermaidClassDialog::saveCurrentClassData()
{
    if (m_lastClassRow < 0) return;
    ClassData data;

    for (int r = 0; r < m_fieldsTable->rowCount(); ++r) {
        QMap<QString, QString> field;
        auto *nameItem = m_fieldsTable->item(r, 0);
        auto *typeItem = m_fieldsTable->item(r, 1);
        auto *visCombo = qobject_cast<QComboBox*>(m_fieldsTable->cellWidget(r, 2));
        auto *staticCheck = qobject_cast<QCheckBox*>(m_fieldsTable->cellWidget(r, 3));
        field["name"] = nameItem ? nameItem->text() : QString();
        field["type"] = typeItem ? typeItem->text() : QString();
        field["visibility"] = visCombo ? visCombo->currentText() : "+";
        field["static"] = (staticCheck && staticCheck->isChecked()) ? "true" : "false";
        data.fields.append(field);
    }

    for (int r = 0; r < m_methodsTable->rowCount(); ++r) {
        QMap<QString, QString> method;
        auto *nameItem = m_methodsTable->item(r, 0);
        auto *retItem = m_methodsTable->item(r, 1);
        auto *paramsItem = m_methodsTable->item(r, 2);
        auto *visCombo = qobject_cast<QComboBox*>(m_methodsTable->cellWidget(r, 3));
        method["name"] = nameItem ? nameItem->text() : QString();
        method["returnType"] = retItem ? retItem->text() : QString();
        method["parameters"] = paramsItem ? paramsItem->text() : QString();
        method["visibility"] = visCombo ? visCombo->currentText() : "+";
        data.methods.append(method);
    }

    m_classData[m_lastClassRow] = data;
}

void MermaidClassDialog::loadClassData(int classRow)
{
    m_fieldsTable->blockSignals(true);
    m_methodsTable->blockSignals(true);
    m_fieldsTable->setRowCount(0);
    m_methodsTable->setRowCount(0);

    if (classRow < 0) {
        m_fieldsTable->blockSignals(false);
        m_methodsTable->blockSignals(false);
        return;
    }

    ClassData data = m_classData.value(classRow);

    for (const auto &field : data.fields) {
        int r = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(r);
        m_fieldsTable->setItem(r, 0, new QTableWidgetItem(field.value("name")));
        m_fieldsTable->setItem(r, 1, new QTableWidgetItem(field.value("type")));
        auto *visCombo = new QComboBox;
        visCombo->addItems({"+", "-", "#", "~"});
        int idx = visCombo->findText(field.value("visibility", "+"));
        if (idx >= 0) visCombo->setCurrentIndex(idx);
        m_fieldsTable->setCellWidget(r, 2, visCombo);
        auto *staticCheck = new QCheckBox;
        staticCheck->setCheckState(field.value("static") == "true" ? Qt::Checked : Qt::Unchecked);
        m_fieldsTable->setCellWidget(r, 3, staticCheck);
    }

    for (const auto &method : data.methods) {
        int r = m_methodsTable->rowCount();
        m_methodsTable->insertRow(r);
        m_methodsTable->setItem(r, 0, new QTableWidgetItem(method.value("name")));
        m_methodsTable->setItem(r, 1, new QTableWidgetItem(method.value("returnType")));
        m_methodsTable->setItem(r, 2, new QTableWidgetItem(method.value("parameters")));
        auto *visCombo = new QComboBox;
        visCombo->addItems({"+", "-", "#", "~"});
        int idx = visCombo->findText(method.value("visibility", "+"));
        if (idx >= 0) visCombo->setCurrentIndex(idx);
        m_methodsTable->setCellWidget(r, 3, visCombo);
    }

    m_fieldsTable->blockSignals(false);
    m_methodsTable->blockSignals(false);
}

void MermaidClassDialog::setupDefaultData()
{
    // Classes
    m_classesTable->insertRow(0);
    m_classesTable->setItem(0, 0, new QTableWidgetItem("Animal"));
    auto *type0 = new QComboBox;
    type0->addItems({"class", "interface", "abstract", "enumeration"});
    m_classesTable->setCellWidget(0, 1, type0);

    m_classesTable->insertRow(1);
    m_classesTable->setItem(1, 0, new QTableWidgetItem("Dog"));
    auto *type1 = new QComboBox;
    type1->addItems({"class", "interface", "abstract", "enumeration"});
    m_classesTable->setCellWidget(1, 1, type1);

    // Animal fields
    ClassData animalData;
    QMap<QString, QString> f1; f1["name"] = "name"; f1["type"] = "string"; f1["visibility"] = "+"; f1["static"] = "false"; animalData.fields.append(f1);
    QMap<QString, QString> f2; f2["name"] = "age"; f2["type"] = "int"; f2["visibility"] = "+"; f2["static"] = "false"; animalData.fields.append(f2);

    // Animal methods
    QMap<QString, QString> m1; m1["name"] = "move"; m1["returnType"] = "void"; m1["parameters"] = ""; m1["visibility"] = "+"; animalData.methods.append(m1);
    m_classData[0] = animalData;

    // Dog fields
    ClassData dogData;
    QMap<QString, QString> df1; df1["name"] = "breed"; df1["type"] = "string"; df1["visibility"] = "+"; df1["static"] = "false"; dogData.fields.append(df1);

    // Dog methods
    QMap<QString, QString> dm1; dm1["name"] = "bark"; dm1["returnType"] = "void"; dm1["parameters"] = ""; dm1["visibility"] = "+"; dogData.methods.append(dm1);
    m_classData[1] = dogData;

    // Relation
    m_relationsTable->insertRow(0);
    QStringList names = {"Animal", "Dog"};
    auto *fromCombo = new QComboBox; fromCombo->addItems(names); fromCombo->setCurrentText("Animal");
    auto *toCombo = new QComboBox; toCombo->addItems(names); toCombo->setCurrentText("Dog");
    auto *typeCombo = new QComboBox; typeCombo->addItems({"-->", "<|--", "..|>", "*--", "o--"});
    typeCombo->setCurrentText("<|--");
    m_relationsTable->setCellWidget(0, 0, fromCombo);
    m_relationsTable->setCellWidget(0, 1, toCombo);
    m_relationsTable->setCellWidget(0, 2, typeCombo);
    m_relationsTable->setItem(0, 3, new QTableWidgetItem("extends"));

    m_classesTable->setCurrentCell(0, 0);
}
