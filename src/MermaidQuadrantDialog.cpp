#include "MermaidQuadrantDialog.h"
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
#include <QGridLayout>
#include <QTimer>
#include <QIcon>
#include <QGuiApplication>
#include <QClipboard>

MermaidQuadrantDialog::MermaidQuadrantDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Quadrant Chart");
    resize(900, 600);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidQuadrantDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidQuadrantDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Title:"));
    m_titleEdit = new QLineEdit(leftPanel);
    m_titleEdit->setPlaceholderText("Chart Title");
    m_titleEdit->setText("Reach and Impact");
    leftLayout->addWidget(m_titleEdit);

    leftLayout->addWidget(new QLabel("Axes:"));
    QGridLayout *axisGrid = new QGridLayout();
    axisGrid->addWidget(new QLabel("X-axis left label:", leftPanel), 0, 0);
    m_xAxisEdit = new QLineEdit(leftPanel);
    m_xAxisEdit->setText("Low Reach");
    axisGrid->addWidget(m_xAxisEdit, 0, 1);
    axisGrid->addWidget(new QLabel("X-axis right label:", leftPanel), 0, 2);
    m_xAxisRightEdit = new QLineEdit(leftPanel);
    m_xAxisRightEdit->setText("High Reach");
    axisGrid->addWidget(m_xAxisRightEdit, 0, 3);
    axisGrid->addWidget(new QLabel("Y-axis bottom label:", leftPanel), 1, 0);
    m_yAxisEdit = new QLineEdit(leftPanel);
    m_yAxisEdit->setText("Low Impact");
    axisGrid->addWidget(m_yAxisEdit, 1, 1);
    axisGrid->addWidget(new QLabel("Y-axis top label:", leftPanel), 1, 2);
    m_yAxisTopEdit = new QLineEdit(leftPanel);
    m_yAxisTopEdit->setText("High Impact");
    axisGrid->addWidget(m_yAxisTopEdit, 1, 3);
    leftLayout->addLayout(axisGrid);

    leftLayout->addWidget(new QLabel("Quadrant labels:"));
    QGridLayout *qGrid = new QGridLayout();
    qGrid->addWidget(new QLabel("Q1 (top-right):", leftPanel), 0, 0);
    m_q1Edit = new QLineEdit(leftPanel);
    m_q1Edit->setText("Quick Wins");
    qGrid->addWidget(m_q1Edit, 0, 1);
    qGrid->addWidget(new QLabel("Q2 (top-left):", leftPanel), 1, 0);
    m_q2Edit = new QLineEdit(leftPanel);
    m_q2Edit->setText("Big Bets");
    qGrid->addWidget(m_q2Edit, 1, 1);
    qGrid->addWidget(new QLabel("Q3 (bottom-left):", leftPanel), 2, 0);
    m_q3Edit = new QLineEdit(leftPanel);
    m_q3Edit->setText("Time Sinks");
    qGrid->addWidget(m_q3Edit, 2, 1);
    qGrid->addWidget(new QLabel("Q4 (bottom-right):", leftPanel), 3, 0);
    m_q4Edit = new QLineEdit(leftPanel);
    m_q4Edit->setText("Thankless Tasks");
    qGrid->addWidget(m_q4Edit, 3, 1);
    leftLayout->addLayout(qGrid);

    leftLayout->addWidget(new QLabel("Points (Label, X 0-1, Y 0-1):"));
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("+Row", leftPanel);
    btnLayout->addWidget(addBtn);
    btnLayout->addStretch();
    leftLayout->addLayout(btnLayout);

    const int delCol = 3;
    m_table = new QTableWidget(3, 4, leftPanel);
    m_table->setHorizontalHeaderLabels({"Label", "X", "Y", "Del"});
    m_table->setItem(0, 0, new QTableWidgetItem("Feature A"));
    m_table->setItem(0, 1, new QTableWidgetItem("0.3"));
    m_table->setItem(0, 2, new QTableWidgetItem("0.8"));
    m_table->setItem(1, 0, new QTableWidgetItem("Feature B"));
    m_table->setItem(1, 1, new QTableWidgetItem("0.8"));
    m_table->setItem(1, 2, new QTableWidgetItem("0.9"));
    m_table->setItem(2, 0, new QTableWidgetItem("Feature C"));
    m_table->setItem(2, 1, new QTableWidgetItem("0.1"));
    m_table->setItem(2, 2, new QTableWidgetItem("0.2"));
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
    splitter->setSizes({420, 480});

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

    auto triggerUpdate = [this]() { schedulePreviewUpdate(); };
    connect(m_titleEdit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_xAxisEdit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_yAxisEdit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_q1Edit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_q2Edit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_q3Edit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_q4Edit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_xAxisRightEdit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_yAxisTopEdit, &QLineEdit::textChanged, this, triggerUpdate);
    connect(m_table, &QTableWidget::itemChanged, this, triggerUpdate);
    connect(addBtn, &QPushButton::clicked, this, [this, addDeleteButton]() {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        addDeleteButton(row);
    });
}

void MermaidQuadrantDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidQuadrantDialog::updatePreview()
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

QString MermaidQuadrantDialog::generatedDiagram() const
{
    return buildDiagram();
}

QString MermaidQuadrantDialog::buildDiagram() const
{
    QString out = "quadrantChart\n";
    QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty())
        out += "    title " + title + "\n";

    out += "    x-axis " + m_xAxisEdit->text().trimmed()
         + " --> " + m_xAxisRightEdit->text().trimmed() + "\n";

    out += "    y-axis " + m_yAxisEdit->text().trimmed()
         + " --> " + m_yAxisTopEdit->text().trimmed() + "\n";

    if (!m_q1Edit->text().trimmed().isEmpty())
        out += "    quadrant-1 " + m_q1Edit->text().trimmed() + "\n";
    if (!m_q2Edit->text().trimmed().isEmpty())
        out += "    quadrant-2 " + m_q2Edit->text().trimmed() + "\n";
    if (!m_q3Edit->text().trimmed().isEmpty())
        out += "    quadrant-3 " + m_q3Edit->text().trimmed() + "\n";
    if (!m_q4Edit->text().trimmed().isEmpty())
        out += "    quadrant-4 " + m_q4Edit->text().trimmed() + "\n";

    for (int r = 0; r < m_table->rowCount(); ++r) {
        QString label = m_table->item(r, 0) ? m_table->item(r, 0)->text().trimmed() : QString();
        QString x = m_table->item(r, 1) ? m_table->item(r, 1)->text().trimmed() : QString();
        QString y = m_table->item(r, 2) ? m_table->item(r, 2)->text().trimmed() : QString();
        if (!label.isEmpty() && !x.isEmpty() && !y.isEmpty())
            out += "    " + label + ": [" + x + ", " + y + "]\n";
    }
    return out;
}