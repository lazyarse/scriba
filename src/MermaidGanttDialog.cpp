#include "MermaidGanttDialog.h"
#include "Preview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QWebEngineView>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHeaderView>
#include <QTimer>
#include <QIcon>
#include <QGuiApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidgetItem>
#include <QMap>

static QString mermaidPreviewHtml(const QString &escaped) {
    return QString(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>body{margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:sans-serif;}"
        ".error{color:#d32f2f;padding:16px;}</style>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "</head><body><div class=\"mermaid\">%1</div>"
        "<script>mermaid.initialize({startOnLoad:false,theme:'default'});"
        "try{mermaid.run({querySelector:'.mermaid'}).catch(function(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';});"
        "}catch(e){document.body.innerHTML='<div class=\"error\">'+e+'</div>';}</script></body></html>"
    ).arg(escaped);
}

MermaidGanttDialog::MermaidGanttDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Gantt Chart");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidGanttDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidGanttDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

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
    QPushButton *removeTaskBtn = new QPushButton("-Task", leftPanel);
    taskBtnLayout->addWidget(addTaskBtn);
    taskBtnLayout->addWidget(removeTaskBtn);
    taskBtnLayout->addStretch();
    leftLayout->addLayout(taskBtnLayout);

    m_taskTable = new QTableWidget(4, 6, leftPanel);
    m_taskTable->setHorizontalHeaderLabels({"ID", "Description", "Start/After", "Duration", "Status", "Section"});
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_taskTable);

    leftLayout->addStretch();

    m_preview = new QWebEngineView(this);
    m_preview->setPage(new PreviewPage(m_preview));

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({350, 550});

    mainLayout->addWidget(splitter);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *copyBtn = buttonBox->addButton("Copy", QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton("Insert", QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedDiagram());
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

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

    populateDefaultRow(0, "a1", "API design", "2026-07-01", "7d", "done", "Backend");
    populateDefaultRow(1, "a2", "DB schema", "after a1", "5d", "done", "Backend");
    populateDefaultRow(2, "a3", "Endpoints", "after a2", "14d", "active", "Frontend");
    populateDefaultRow(3, "b1", "UI components", "2026-07-10", "10d", "", "Frontend");

    connect(addTaskBtn, &QPushButton::clicked, this, [this]() {
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
        schedulePreviewUpdate();
    });
    connect(removeTaskBtn, &QPushButton::clicked, this, [this]() {
        if (m_taskTable->rowCount() > 1) {
            m_taskTable->removeRow(m_taskTable->rowCount() - 1);
            schedulePreviewUpdate();
        }
    });
}

void MermaidGanttDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidGanttDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped));
}

QString MermaidGanttDialog::generatedDiagram() const
{
    return buildDiagram();
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
