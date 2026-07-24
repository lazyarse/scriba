#include "MermaidPieDialog.h"
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
#include <QJsonDocument>
#include <QJsonObject>

MermaidPieDialog::MermaidPieDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Pie Chart");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidPieDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidPieDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

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
    QPushButton *removeBtn = new QPushButton("-Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    m_table = new QTableWidget(2, 2, leftPanel);
    m_table->setHorizontalHeaderLabels({"Label", "Value"});
    m_table->setItem(0, 0, new QTableWidgetItem("Alpha"));
    m_table->setItem(0, 1, new QTableWidgetItem("30"));
    m_table->setItem(1, 0, new QTableWidgetItem("Beta"));
    m_table->setItem(1, 1, new QTableWidgetItem("70"));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(28);
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

    connect(m_titleEdit, &QLineEdit::textChanged, this, &MermaidPieDialog::schedulePreviewUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidPieDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        m_table->insertRow(m_table->rowCount());
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        if (m_table->rowCount() > 1)
            m_table->removeRow(m_table->rowCount() - 1);
    });
}

void MermaidPieDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidPieDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");

    QString html = QString(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>"
        "body{margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:sans-serif;}"
        ".mermaid{max-width:100%;}"
        ".error{color:#d32f2f;padding:16px;font-size:14px;}"
        "</style>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "</head><body>"
        "<div class=\"mermaid\">%1</div>"
        "<script>"
        "mermaid.initialize({startOnLoad:false,theme:'default'});"
        "try{"
        "mermaid.run({querySelector:'.mermaid'}).catch(function(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';"
        "});"
        "}catch(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';"
        "}"
        "</script></body></html>"
    ).arg(escaped);

    m_preview->setHtml(html);
}

QString MermaidPieDialog::generatedDiagram() const
{
    return buildDiagram();
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