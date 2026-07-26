#include "MermaidPieDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>

MermaidPieDialog::MermaidPieDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Pie Chart", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidPieDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit(leftPanel);
    m_titleEdit->setPlaceholderText("My Pie Chart");
    leftLayout->addWidget(m_titleEdit);

    leftLayout->addWidget(new QLabel("Slices:"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    const int delCol = 2;
    m_table = new QTableWidget(2, 3, leftPanel);
    m_table->setHorizontalHeaderLabels({"Label", "Value", "Del"});
    m_table->setItem(0, 0, new QTableWidgetItem("Alpha"));
    m_table->setItem(0, 1, new QTableWidgetItem("30"));
    m_table->setItem(1, 0, new QTableWidgetItem("Beta"));
    m_table->setItem(1, 1, new QTableWidgetItem("70"));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_table->setColumnWidth(delCol, 32);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_table);

    addDeleteButton(m_table, delCol, 0);
    addDeleteButton(m_table, delCol, 1);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidPieDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidPieDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(""));
        m_table->setItem(row, 1, new QTableWidgetItem("0"));
        addDeleteButton(m_table, delCol, row);
        schedulePreviewUpdate();
    });
}

QString MermaidPieDialog::buildDiagram() const
{
    QString out;
    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty())
        out += "pie title " + title + "\n";
    else
        out += "pie\n";

    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *labelItem = m_table->item(r, 0);
        QTableWidgetItem *valueItem = m_table->item(r, 1);
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString value = valueItem ? valueItem->text().trimmed() : QString();
        if (!label.isEmpty() && !value.isEmpty())
            out += "    \"" + label + "\" : " + value + "\n";
    }

    return out;
}
