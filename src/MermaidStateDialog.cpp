#include "MermaidStateDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>

MermaidStateDialog::MermaidStateDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid State Diagram", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidStateDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("States:"));
    QHBoxLayout *stateBtnLayout = new QHBoxLayout();
    QPushButton *addStateBtn = new QPushButton("+State", leftPanel);
    stateBtnLayout->addWidget(addStateBtn);
    stateBtnLayout->addStretch();
    leftLayout->addLayout(stateBtnLayout);

    const int stateDelCol = 2;
    m_stateTable = new QTableWidget(4, 3, leftPanel);
    m_stateTable->setHorizontalHeaderLabels({"Name", "Description", "Del"});
    m_stateTable->setItem(0, 0, new QTableWidgetItem("[*]"));
    m_stateTable->setItem(1, 0, new QTableWidgetItem("Idle"));
    m_stateTable->setItem(2, 0, new QTableWidgetItem("Processing"));
    m_stateTable->setItem(3, 0, new QTableWidgetItem("Done"));
    m_stateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stateTable->horizontalHeader()->setSectionResizeMode(stateDelCol, QHeaderView::Fixed);
    m_stateTable->setColumnWidth(stateDelCol, 32);
    m_stateTable->verticalHeader()->setDefaultSectionSize(28);

    for (int r = 0; r < m_stateTable->rowCount(); ++r)
        addDeleteButton(m_stateTable, stateDelCol, r, [this](){ refreshTransitionCombos(); });

    leftLayout->addWidget(m_stateTable);

    leftLayout->addWidget(new QLabel("Transitions:"));
    QHBoxLayout *transBtnLayout = new QHBoxLayout();
    QPushButton *addTransBtn = new QPushButton("+Transition", leftPanel);
    transBtnLayout->addWidget(addTransBtn);
    transBtnLayout->addStretch();
    leftLayout->addLayout(transBtnLayout);

    const int transDelCol = 3;
    m_transitionTable = new QTableWidget(4, 4, leftPanel);
    m_transitionTable->setHorizontalHeaderLabels({"From", "To", "Label", "Del"});
    m_transitionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transitionTable->horizontalHeader()->setSectionResizeMode(transDelCol, QHeaderView::Fixed);
    m_transitionTable->setColumnWidth(transDelCol, 32);
    m_transitionTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_transitionTable);

    for (int r = 0; r < m_transitionTable->rowCount(); ++r)
        addDeleteButton(m_transitionTable, transDelCol, r);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_stateTable, &QTableWidget::itemChanged, this, &MermaidStateDialog::onStateChanged);
    connect(m_transitionTable, &QTableWidget::itemChanged, this, &MermaidStateDialog::schedulePreviewUpdate);

    connect(addStateBtn, &QPushButton::clicked, this, [this]() {
        int row = m_stateTable->rowCount();
        m_stateTable->insertRow(row);
        addDeleteButton(m_stateTable, stateDelCol, row, [this](){ refreshTransitionCombos(); });
        refreshTransitionCombos();
        schedulePreviewUpdate();
    });
    connect(addTransBtn, &QPushButton::clicked, this, [this]() {
        int row = m_transitionTable->rowCount();
        m_transitionTable->insertRow(row);
        addDeleteButton(m_transitionTable, transDelCol, row);
        refreshTransitionCombos();
        schedulePreviewUpdate();
    });

    refreshTransitionCombos();
}

void MermaidStateDialog::onStateChanged()
{
    refreshTransitionCombos();
    schedulePreviewUpdate();
}

void MermaidStateDialog::refreshTransitionCombos()
{
    QStringList stateNames;
    for (int r = 0; r < m_stateTable->rowCount(); ++r) {
        QTableWidgetItem *item = m_stateTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            stateNames.append(item->text().trimmed());
    }
    populateComboColumns(m_transitionTable, {0, 1}, stateNames);
}

QString MermaidStateDialog::buildDiagram() const
{
    QString out = "stateDiagram-v2\n";
    for (int r = 0; r < m_transitionTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_transitionTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_transitionTable->cellWidget(r, 1));
        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QTableWidgetItem *labelItem = m_transitionTable->item(r, 2);
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + " --> " + to;
        if (!label.isEmpty())
            out += " : " + label;
        out += "\n";
    }
    return out;
}
