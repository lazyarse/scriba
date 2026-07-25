#include "MermaidJourneyDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QSpinBox>
#include <QPalette>

MermaidJourneyDialog::MermaidJourneyDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Journey", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidJourneyDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit(leftPanel);
    m_titleEdit->setPlaceholderText("My Day");
    leftLayout->addWidget(m_titleEdit);

    leftLayout->addWidget(new QLabel("Tasks:"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    const int delCol = 4;
    m_table = new QTableWidget(4, 5, leftPanel);
    m_table->setHorizontalHeaderLabels({"Section", "Task Name", "Score", "Actors", "Del"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_table->setColumnWidth(delCol, 32);
    m_table->verticalHeader()->setDefaultSectionSize(28);

    auto setRow = [&](int row, const QString &section, const QString &task, int score, const QString &actors) {
        m_table->setItem(row, 0, new QTableWidgetItem(section));
        m_table->setItem(row, 1, new QTableWidgetItem(task));
        QSpinBox *spin = new QSpinBox(m_table);
        spin->setRange(1, 7);
        spin->setValue(score);
        m_table->setCellWidget(row, 2, spin);
        m_table->setItem(row, 3, new QTableWidgetItem(actors));
    };

    setRow(0, "Morning", "Wake up", 5, "Me");
    setRow(1, "Morning", "Shower", 3, "Me");
    setRow(2, "Work", "Coding", 7, "Me, Team");
    setRow(3, "Work", "Meetings", 2, "Me, Boss");

    auto addDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", palette().color(QPalette::WindowText), 16), "", m_table);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_table->setCellWidget(row, delCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_table->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_table->rowCount() > 1)
                m_table->removeRow(row);
            schedulePreviewUpdate();
        });
    };
    for (int r = 0; r < m_table->rowCount(); ++r)
        addDeleteButton(r);

    leftLayout->addWidget(m_table);
    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidJourneyDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidJourneyDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this, addDeleteButton]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(row);
    });
}

QString MermaidJourneyDialog::buildDiagram() const
{
    QString title = m_titleEdit->text().trimmed();
    QString out = "journey\n";
    if (!title.isEmpty())
        out += "    title " + title + "\n";

    QString currentSection;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString section = m_table->item(r, 0) ? m_table->item(r, 0)->text().trimmed() : QString();
        QString task = m_table->item(r, 1) ? m_table->item(r, 1)->text().trimmed() : QString();
        QSpinBox *spin = qobject_cast<QSpinBox*>(m_table->cellWidget(r, 2));
        int score = spin ? spin->value() : 5;
        QString actors = m_table->item(r, 3) ? m_table->item(r, 3)->text().trimmed() : QString();
        if (section.isEmpty() && task.isEmpty()) continue;

        if (section != currentSection && !section.isEmpty()) {
            out += "    section " + section + "\n";
            currentSection = section;
        }
        if (!task.isEmpty())
            out += "        " + task + ": " + QString::number(score) + ": " + actors + "\n";
    }
    return out;
}
