#include "MermaidSankeyDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>

MermaidSankeyDialog::MermaidSankeyDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Sankey Diagram", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidSankeyDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Links (Source, Target, Value):"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    const int delCol = 3;
    m_table = new QTableWidget(3, 4, leftPanel);
    m_table->setHorizontalHeaderLabels({"Source", "Target", "Value", "Del"});
    m_table->setItem(0, 0, new QTableWidgetItem("Revenue"));
    m_table->setItem(0, 1, new QTableWidgetItem("Product Sales"));
    m_table->setItem(0, 2, new QTableWidgetItem("600"));
    m_table->setItem(1, 0, new QTableWidgetItem("Revenue"));
    m_table->setItem(1, 1, new QTableWidgetItem("Services"));
    m_table->setItem(1, 2, new QTableWidgetItem("300"));
    m_table->setItem(2, 0, new QTableWidgetItem("Product Sales"));
    m_table->setItem(2, 1, new QTableWidgetItem("COGS"));
    m_table->setItem(2, 2, new QTableWidgetItem("250"));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_table->setColumnWidth(delCol, 32);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_table);

    for (int r = 0; r < m_table->rowCount(); ++r)
        addDeleteButton(m_table, delCol, r);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_table, &QTableWidget::itemChanged, this, &MermaidSankeyDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(m_table, delCol, row);
        schedulePreviewUpdate();
    });
}

QString MermaidSankeyDialog::buildDiagram() const
{
    QString out = "sankey-beta\n";
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString src = m_table->item(r, 0) ? m_table->item(r, 0)->text().trimmed() : QString();
        QString tgt = m_table->item(r, 1) ? m_table->item(r, 1)->text().trimmed() : QString();
        QString val = m_table->item(r, 2) ? m_table->item(r, 2)->text().trimmed() : QString();
        if (!src.isEmpty() && !tgt.isEmpty() && !val.isEmpty())
            out += "    " + src + "," + tgt + "," + val + "\n";
    }
    return out;
}
