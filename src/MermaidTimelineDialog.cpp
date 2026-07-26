#include "MermaidTimelineDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>

MermaidTimelineDialog::MermaidTimelineDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Timeline", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidTimelineDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit(leftPanel);
    m_titleEdit->setPlaceholderText("My Timeline");
    leftLayout->addWidget(m_titleEdit);

    leftLayout->addWidget(new QLabel("Entries (Section, Event):"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    const int delCol = 2;
    m_table = new QTableWidget(3, 3, leftPanel);
    m_table->setHorizontalHeaderLabels({"Section", "Event", "Del"});
    m_table->setItem(0, 0, new QTableWidgetItem("Q1 2026"));
    m_table->setItem(0, 1, new QTableWidgetItem("Launch v1.0"));
    m_table->setItem(1, 0, new QTableWidgetItem("Q2 2026"));
    m_table->setItem(1, 1, new QTableWidgetItem("Template library"));
    m_table->setItem(2, 0, new QTableWidgetItem("Q2 2026"));
    m_table->setItem(2, 1, new QTableWidgetItem("VS Code extension"));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_table->setColumnWidth(delCol, 32);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_table);

    for (int r = 0; r < m_table->rowCount(); ++r)
        addDeleteButton(m_table, delCol, r);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidTimelineDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidTimelineDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(m_table, delCol, row);
        schedulePreviewUpdate();
    });
}

QString MermaidTimelineDialog::buildDiagram() const
{
    QString title = m_titleEdit->text().trimmed();
    QString out = "timeline\n";
    if (!title.isEmpty())
        out += "    title " + title + "\n";

    QString currentSection;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString section = m_table->item(r, 0) ? m_table->item(r, 0)->text().trimmed() : QString();
        QString event = m_table->item(r, 1) ? m_table->item(r, 1)->text().trimmed() : QString();
        if (section.isEmpty() && event.isEmpty()) continue;

        if (section != currentSection) {
            if (!section.isEmpty()) {
                out += "    " + section + "\n";
                currentSection = section;
            }
        }
        if (!event.isEmpty())
            out += "            : " + event + "\n";
    }
    return out;
}
