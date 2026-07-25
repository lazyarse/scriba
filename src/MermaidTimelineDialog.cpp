#include "MermaidTimelineDialog.h"
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
#include <QIcon>
#include <QGuiApplication>
#include <QClipboard>

MermaidTimelineDialog::MermaidTimelineDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Timeline");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidTimelineDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidTimelineDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

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

    auto addDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton("\u00d7", m_table);
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
    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidTimelineDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidTimelineDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this, addDeleteButton]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(row);
    });
}

void MermaidTimelineDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidTimelineDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    QString html = QString(
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
    m_preview->setHtml(html);
}

QString MermaidTimelineDialog::generatedDiagram() const
{
    return buildDiagram();
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