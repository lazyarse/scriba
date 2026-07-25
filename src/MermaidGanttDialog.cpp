#include "MermaidGanttDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidgetItem>
#include <QMap>

MermaidGanttDialog::MermaidGanttDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Gantt Chart", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidGanttDialog::setupUi()
{
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit(leftPanel);
    m_titleEdit->setPlaceholderText("Project Plan");
    titleLayout->addWidget(m_titleEdit);
    leftLayout->addLayout(titleLayout);

    QHBoxLayout *dateFormatLayout = new QHBoxLayout();
    dateFormatLayout->addWidget(new QLabel("Date format:"));
    m_dateFormatCombo = new QComboBox(leftPanel);
    m_dateFormatCombo->addItems({"YYYY-MM-DD", "DD/MM/YYYY", "MM-DD-YYYY"});
    dateFormatLayout->addWidget(m_dateFormatCombo);
    leftLayout->addLayout(dateFormatLayout);

    m_excludeWeekendsCheck = new QCheckBox("Exclude weekends", leftPanel);
    m_excludeWeekendsCheck->setChecked(true);
    leftLayout->addWidget(m_excludeWeekendsCheck);

    leftLayout->addWidget(new QLabel("Tasks:"));
    QHBoxLayout *taskBtnLayout = new QHBoxLayout();
    QPushButton *addTaskBtn = new QPushButton("+Task", leftPanel);
    taskBtnLayout->addWidget(addTaskBtn);
    taskBtnLayout->addStretch();
    leftLayout->addLayout(taskBtnLayout);

    const int delCol = 6;
    m_taskTable = new QTableWidget(4, 7, leftPanel);
    m_taskTable->setHorizontalHeaderLabels({"ID", "Description", "Start/After", "Duration", "Status", "Section", "Del"});
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->horizontalHeader()->setSectionResizeMode(delCol, QHeaderView::Fixed);
    m_taskTable->setColumnWidth(delCol, 32);
    m_taskTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_taskTable);

    leftLayout->addStretch();

    setupMainLayout(leftPanel, leftLayout, {350, 550});

    connect(m_titleEdit, &QLineEdit::textChanged,
            this, &MermaidGanttDialog::schedulePreviewUpdate);
    connect(m_dateFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MermaidGanttDialog::schedulePreviewUpdate);
    connect(m_excludeWeekendsCheck, &QCheckBox::toggled,
            this, &MermaidGanttDialog::schedulePreviewUpdate);
    connect(m_taskTable, &QTableWidget::itemChanged,
            this, &MermaidGanttDialog::schedulePreviewUpdate);

    auto populateDefaultRow = [this](int row, const QString &id, const QString &desc,
                                     const QString &start, const QString &duration,
                                     const QString &status, const QString &section) {
        m_taskTable->setItem(row, 0, new QTableWidgetItem(id));
        m_taskTable->setItem(row, 1, new QTableWidgetItem(desc));
        m_taskTable->setItem(row, 2, new QTableWidgetItem(start));
        m_taskTable->setItem(row, 3, new QTableWidgetItem(duration));
        m_taskTable->setItem(row, 5, new QTableWidgetItem(section));

        QComboBox *statusCombo = new QComboBox(m_taskTable);
        statusCombo->addItems({"", "done", "active", "crit", "milestone"});
        int idx = statusCombo->findText(status);
        if (idx >= 0) statusCombo->setCurrentIndex(idx);
        m_taskTable->setCellWidget(row, 4, statusCombo);
        connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidGanttDialog::schedulePreviewUpdate);
    };

    auto addDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", iconColor(), 16), "", m_taskTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_taskTable->setCellWidget(row, delCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_taskTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_taskTable->rowCount() > 1)
                m_taskTable->removeRow(row);
            schedulePreviewUpdate();
        });
    };

    populateDefaultRow(0, "a1", "API design", "2026-07-01", "7d", "done", "Backend");
    populateDefaultRow(1, "a2", "DB schema", "after a1", "5d", "done", "Backend");
    populateDefaultRow(2, "a3", "Endpoints", "after a2", "14d", "active", "Frontend");
    populateDefaultRow(3, "b1", "UI components", "2026-07-10", "10d", "", "Frontend");
    for (int r = 0; r < m_taskTable->rowCount(); ++r)
        addDeleteButton(r);

    connect(addTaskBtn, &QPushButton::clicked, this, [this, addDeleteButton]() {
        int row = m_taskTable->rowCount();
        m_taskTable->insertRow(row);
        m_taskTable->setItem(row, 0, new QTableWidgetItem(""));
        m_taskTable->setItem(row, 1, new QTableWidgetItem(""));
        m_taskTable->setItem(row, 2, new QTableWidgetItem(""));
        m_taskTable->setItem(row, 3, new QTableWidgetItem(""));
        m_taskTable->setItem(row, 5, new QTableWidgetItem(""));
        QComboBox *statusCombo = new QComboBox(m_taskTable);
        statusCombo->addItems({"", "done", "active", "crit", "milestone"});
        m_taskTable->setCellWidget(row, 4, statusCombo);
        connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MermaidGanttDialog::schedulePreviewUpdate);
        addDeleteButton(row);
        schedulePreviewUpdate();
    });
}

QString MermaidGanttDialog::buildDiagram() const
{
    QString out = "gantt\n";

    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty())
        out += "    title " + title + "\n";

    out += "    dateFormat " + m_dateFormatCombo->currentText() + "\n";

    if (m_excludeWeekendsCheck->isChecked())
        out += "    excludes weekends\n";

    QMap<QString, QList<int>> sections;
    for (int r = 0; r < m_taskTable->rowCount(); ++r) {
        QTableWidgetItem *sectionItem = m_taskTable->item(r, 5);
        QString section = sectionItem ? sectionItem->text().trimmed() : QString();
        if (section.isEmpty()) section = "(none)";
        sections[section].append(r);
    }

    for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
        out += "    section " + it.key() + "\n";
        for (int r : it.value()) {
            QTableWidgetItem *idItem = m_taskTable->item(r, 0);
            QTableWidgetItem *descItem = m_taskTable->item(r, 1);
            QTableWidgetItem *startItem = m_taskTable->item(r, 2);
            QTableWidgetItem *durationItem = m_taskTable->item(r, 3);
            QComboBox *statusBox = qobject_cast<QComboBox*>(m_taskTable->cellWidget(r, 4));

            QString id = idItem ? idItem->text().trimmed() : QString();
            QString desc = descItem ? descItem->text().trimmed() : QString();
            QString start = startItem ? startItem->text().trimmed() : QString();
            QString duration = durationItem ? durationItem->text().trimmed() : QString();
            QString status = statusBox ? statusBox->currentText() : QString();

            if (id.isEmpty()) continue;

            QString taskLine = "        " + desc;
            if (!status.isEmpty())
                taskLine += " :" + status + ", ";
            else
                taskLine += " : ";
            taskLine += id + ", " + start + ", " + duration;
            out += taskLine + "\n";
        }
    }

    return out;
}
