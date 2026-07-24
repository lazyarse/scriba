#include "MermaidJourneyDialog.h"
#include "Preview.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHeaderView>
#include <QTimer>
#include <QSpinBox>
#include <QIcon>
#include <QGuiApplication>
#include <QClipboard>

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

MermaidJourneyDialog::MermaidJourneyDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Journey");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidJourneyDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidJourneyDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

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
    QPushButton *removeBtn = new QPushButton("-Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    m_table = new QTableWidget(4, 4, leftPanel);
    m_table->setHorizontalHeaderLabels({"Section", "Task Name", "Score", "Actors"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
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

    leftLayout->addWidget(m_table);
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
    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidJourneyDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidJourneyDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        m_table->insertRow(m_table->rowCount());
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        if (m_table->rowCount() > 1)
            m_table->removeRow(m_table->rowCount() - 1);
    });
}

void MermaidJourneyDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidJourneyDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped));
}

QString MermaidJourneyDialog::generatedDiagram() const
{
    return buildDiagram();
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
