#include "MermaidStateDialog.h"
#include "Preview.h"
#include "StaticHelpers.h"
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
#include <QPalette>
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

MermaidStateDialog::MermaidStateDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid State Diagram");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidStateDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidStateDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("States:"));
    QHBoxLayout *stateBtnLayout = new QHBoxLayout();
    QPushButton *addStateBtn = new QPushButton("+State", leftPanel);
    stateBtnLayout->addWidget(addStateBtn);
    stateBtnLayout->addStretch();
    leftLayout->addLayout(stateBtnLayout);

    const int stateDelCol = 2;
    m_stateTable = new QTableWidget(4, 3, leftPanel);
    m_stateTable->setHorizontalHeaderLabels({"Name", "Description", "Del"});
    m_stateTable->setItem(0, 0, new QTableWidgetItem("[*]"));
    m_stateTable->setItem(1, 0, new QTableWidgetItem("Idle"));
    m_stateTable->setItem(2, 0, new QTableWidgetItem("Processing"));
    m_stateTable->setItem(3, 0, new QTableWidgetItem("Done"));
    m_stateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stateTable->horizontalHeader()->setSectionResizeMode(stateDelCol, QHeaderView::Fixed);
    m_stateTable->setColumnWidth(stateDelCol, 32);
    m_stateTable->verticalHeader()->setDefaultSectionSize(28);

    auto addStateDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", palette().color(QPalette::WindowText), 16), "", m_stateTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_stateTable->setCellWidget(row, stateDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_stateTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_stateTable->rowCount() > 1) {
                m_stateTable->removeRow(row);
                refreshTransitionCombos();
                schedulePreviewUpdate();
            }
        });
    };
    for (int r = 0; r < m_stateTable->rowCount(); ++r)
        addStateDeleteButton(r);

    leftLayout->addWidget(m_stateTable);

    leftLayout->addWidget(new QLabel("Transitions:"));
    QHBoxLayout *transBtnLayout = new QHBoxLayout();
    QPushButton *addTransBtn = new QPushButton("+Transition", leftPanel);
    transBtnLayout->addWidget(addTransBtn);
    transBtnLayout->addStretch();
    leftLayout->addLayout(transBtnLayout);

    const int transDelCol = 3;
    m_transitionTable = new QTableWidget(4, 4, leftPanel);
    m_transitionTable->setHorizontalHeaderLabels({"From", "To", "Label", "Del"});
    m_transitionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transitionTable->horizontalHeader()->setSectionResizeMode(transDelCol, QHeaderView::Fixed);
    m_transitionTable->setColumnWidth(transDelCol, 32);
    m_transitionTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_transitionTable);

    auto addTransitionDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", palette().color(QPalette::WindowText), 16), "", m_transitionTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_transitionTable->setCellWidget(row, transDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_transitionTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_transitionTable->rowCount() > 1)
                m_transitionTable->removeRow(row);
            schedulePreviewUpdate();
        });
    };
    for (int r = 0; r < m_transitionTable->rowCount(); ++r)
        addTransitionDeleteButton(r);

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
    connect(m_stateTable, &QTableWidget::itemChanged, this, &MermaidStateDialog::onStateChanged);
    connect(m_transitionTable, &QTableWidget::itemChanged, this, &MermaidStateDialog::schedulePreviewUpdate);

    connect(addStateBtn, &QPushButton::clicked, this, [this, addStateDeleteButton]() {
        int row = m_stateTable->rowCount();
        m_stateTable->insertRow(row);
        addStateDeleteButton(row);
        refreshTransitionCombos();
        schedulePreviewUpdate();
    });
    connect(addTransBtn, &QPushButton::clicked, this, [this, addTransitionDeleteButton]() {
        int row = m_transitionTable->rowCount();
        m_transitionTable->insertRow(row);
        addTransitionDeleteButton(row);
        refreshTransitionCombos();
        schedulePreviewUpdate();
    });

    refreshTransitionCombos();
}

void MermaidStateDialog::onStateChanged()
{
    refreshTransitionCombos();
    schedulePreviewUpdate();
}

void MermaidStateDialog::refreshTransitionCombos()
{
    QStringList stateNames;
    for (int r = 0; r < m_stateTable->rowCount(); ++r) {
        QTableWidgetItem *item = m_stateTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            stateNames.append(item->text().trimmed());
    }
    for (int r = 0; r < m_transitionTable->rowCount(); ++r) {
        for (int col : {0, 1}) {
            QComboBox *box = qobject_cast<QComboBox*>(m_transitionTable->cellWidget(r, col));
            if (!box) {
                box = new QComboBox(m_transitionTable);
                m_transitionTable->setCellWidget(r, col, box);
                connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MermaidStateDialog::schedulePreviewUpdate);
            }
            QString cur = box->currentText();
            box->blockSignals(true);
            box->clear();
            box->addItems(stateNames);
            int idx = box->findText(cur);
            if (idx >= 0) box->setCurrentIndex(idx);
            box->blockSignals(false);
        }
    }
}

void MermaidStateDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidStateDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped));
}

QString MermaidStateDialog::generatedDiagram() const
{
    return buildDiagram();
}

QString MermaidStateDialog::buildDiagram() const
{
    QString out = "stateDiagram-v2\n";
    for (int r = 0; r < m_transitionTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_transitionTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_transitionTable->cellWidget(r, 1));
        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QTableWidgetItem *labelItem = m_transitionTable->item(r, 2);
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        if (from.isEmpty() || to.isEmpty()) continue;
        out += "    " + from + " --> " + to;
        if (!label.isEmpty())
            out += " : " + label;
        out += "\n";
    }
    return out;
}
