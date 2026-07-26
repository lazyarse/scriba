#include "MermaidSequenceDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidgetItem>

MermaidSequenceDialog::MermaidSequenceDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Sequence Diagram", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidSequenceDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Participants:"));
    QHBoxLayout *participantBtnLayout = new QHBoxLayout();
    QPushButton *addParticipantBtn = new QPushButton("+Participant", leftPanel);
    participantBtnLayout->addWidget(addParticipantBtn);
    participantBtnLayout->addStretch();
    leftLayout->addLayout(participantBtnLayout);

    const int partDelCol = 2;
    m_participantTable = new QTableWidget(2, 3, leftPanel);
    m_participantTable->setHorizontalHeaderLabels({"Name", "Alias", "Del"});
    m_participantTable->setItem(0, 0, new QTableWidgetItem("Alice"));
    m_participantTable->setItem(1, 0, new QTableWidgetItem("Bob"));
    m_participantTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_participantTable->horizontalHeader()->setSectionResizeMode(partDelCol, QHeaderView::Fixed);
    m_participantTable->setColumnWidth(partDelCol, 32);
    m_participantTable->verticalHeader()->setDefaultSectionSize(28);

    addDeleteButton(m_participantTable, partDelCol, 0, [this](){ refreshMessageCombos(); });
    addDeleteButton(m_participantTable, partDelCol, 1, [this](){ refreshMessageCombos(); });

    leftLayout->addWidget(m_participantTable);

    leftLayout->addWidget(new QLabel("Messages:"));
    QHBoxLayout *messageBtnLayout = new QHBoxLayout();
    QPushButton *addMessageBtn = new QPushButton("+Message", leftPanel);
    messageBtnLayout->addWidget(addMessageBtn);
    messageBtnLayout->addStretch();
    leftLayout->addLayout(messageBtnLayout);

    const int msgDelCol = 4;
    m_messageTable = new QTableWidget(2, 5, leftPanel);
    m_messageTable->setHorizontalHeaderLabels({"From", "To", "Label", "Arrow", "Del"});
    m_messageTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_messageTable->horizontalHeader()->setSectionResizeMode(msgDelCol, QHeaderView::Fixed);
    m_messageTable->setColumnWidth(msgDelCol, 32);
    m_messageTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_messageTable);

    addDeleteButton(m_messageTable, msgDelCol, 0);
    addDeleteButton(m_messageTable, msgDelCol, 1);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_participantTable, &QTableWidget::itemChanged,
            this, &MermaidSequenceDialog::onParticipantChanged);
    connect(m_messageTable, &QTableWidget::itemChanged,
            this, &MermaidSequenceDialog::schedulePreviewUpdate);

    connect(addParticipantBtn, &QPushButton::clicked, this, [this]() {
        int row = m_participantTable->rowCount();
        m_participantTable->insertRow(row);
        addDeleteButton(m_participantTable, partDelCol, row, [this](){ refreshMessageCombos(); });
        refreshMessageCombos();
        schedulePreviewUpdate();
    });
    connect(addMessageBtn, &QPushButton::clicked, this, [this]() {
        int row = m_messageTable->rowCount();
        m_messageTable->insertRow(row);
        m_messageTable->setItem(row, 2, new QTableWidgetItem(""));
        addDeleteButton(m_messageTable, msgDelCol, row);
        refreshMessageCombos();
        schedulePreviewUpdate();
    });

    refreshMessageCombos();

    m_messageTable->setItem(0, 2, new QTableWidgetItem("Hello"));
    QComboBox *arrow0 = qobject_cast<QComboBox*>(m_messageTable->cellWidget(0, 3));
    if (arrow0) arrow0->setCurrentIndex(0);

    m_messageTable->setItem(1, 2, new QTableWidgetItem("Hi"));
    QComboBox *arrow1 = qobject_cast<QComboBox*>(m_messageTable->cellWidget(1, 3));
    if (arrow1) arrow1->setCurrentIndex(1);
}

void MermaidSequenceDialog::onParticipantChanged()
{
    refreshMessageCombos();
    schedulePreviewUpdate();
}

void MermaidSequenceDialog::refreshMessageCombos()
{
    QStringList names;
    for (int r = 0; r < m_participantTable->rowCount(); ++r) {
        QTableWidgetItem *item = m_participantTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            names.append(item->text().trimmed());
    }
    populateComboColumns(m_messageTable, {0, 1}, names);
    for (int r = 0; r < m_messageTable->rowCount(); ++r) {
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 3));
        if (!arrowBox) {
            arrowBox = new QComboBox(m_messageTable);
            arrowBox->addItems({"->>", "-->>", "-x", "--)", "->", "-->"});
            m_messageTable->setCellWidget(r, 3, arrowBox);
            connect(arrowBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidSequenceDialog::schedulePreviewUpdate);
        }
    }
}

QString MermaidSequenceDialog::buildDiagram() const
{
    QString out = "sequenceDiagram\n";

    for (int r = 0; r < m_participantTable->rowCount(); ++r) {
        QTableWidgetItem *nameItem = m_participantTable->item(r, 0);
        QTableWidgetItem *aliasItem = m_participantTable->item(r, 1);
        QString name = nameItem ? nameItem->text().trimmed() : QString();
        QString alias = aliasItem ? aliasItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;
        out += "    participant " + name;
        if (!alias.isEmpty())
            out += " as " + alias;
        out += "\n";
    }

    for (int r = 0; r < m_messageTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 1));
        QTableWidgetItem *labelItem = m_messageTable->item(r, 2);
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 3));

        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString arrow = arrowBox ? arrowBox->currentText() : "->>";

        if (from.isEmpty() || to.isEmpty()) continue;

        out += "    " + from + arrow + to + ": " + label + "\n";
    }

    return out;
}
