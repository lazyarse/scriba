#include "MermaidSankeyDialog.h"
#include "Preview.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QWebEngineView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHeaderView>
#include <QTimer>
#include <QIcon>
#include <QPalette>
#include <QGuiApplication>
#include <QClipboard>

MermaidSankeyDialog::MermaidSankeyDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Sankey Diagram");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidSankeyDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidSankeyDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

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
    connect(m_table, &QTableWidget::itemChanged, this, &MermaidSankeyDialog::schedulePreviewUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this, addDeleteButton]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(row);
    });
}

void MermaidSankeyDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidSankeyDialog::updatePreview()
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

QString MermaidSankeyDialog::generatedDiagram() const
{
    return buildDiagram();
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