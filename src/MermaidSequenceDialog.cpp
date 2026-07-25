#include "MermaidSequenceDialog.h"
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
#include <QTableWidgetItem>

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

MermaidSequenceDialog::MermaidSequenceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mermaid Sequence Diagram");
    resize(900, 550);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidSequenceDialog::updatePreview);

    setupUi();
    updatePreview();
}

void MermaidSequenceDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Participants:"));
    QHBoxLayout *participantBtnLayout = new QHBoxLayout();
    QPushButton *addParticipantBtn = new QPushButton("+Participant", leftPanel);
    participantBtnLayout->addWidget(addParticipantBtn);
    participantBtnLayout->addStretch();
    leftLayout->addLayout(participantBtnLayout);

    const int partDelCol = 2;
    m_participantTable = new QTableWidget(2, 3, leftPanel);
    m_participantTable->setHorizontalHeaderLabels({"Name", "Alias", "Del"});
    m_participantTable->setItem(0, 0, new QTableWidgetItem("Alice"));
    m_participantTable->setItem(1, 0, new QTableWidgetItem("Bob"));
    m_participantTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_participantTable->horizontalHeader()->setSectionResizeMode(partDelCol, QHeaderView::Fixed);
    m_participantTable->setColumnWidth(partDelCol, 32);
    m_participantTable->verticalHeader()->setDefaultSectionSize(28);

    auto addParticipantDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton("\u00d7", m_participantTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_participantTable->setCellWidget(row, partDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_participantTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_participantTable->rowCount() > 1) {
                m_participantTable->removeRow(row);
                refreshMessageCombos();
                schedulePreviewUpdate();
            }
        });
    };
    addParticipantDeleteButton(0);
    addParticipantDeleteButton(1);

    leftLayout->addWidget(m_participantTable);

    leftLayout->addWidget(new QLabel("Messages:"));
    QHBoxLayout *messageBtnLayout = new QHBoxLayout();
    QPushButton *addMessageBtn = new QPushButton("+Message", leftPanel);
    messageBtnLayout->addWidget(addMessageBtn);
    messageBtnLayout->addStretch();
    leftLayout->addLayout(messageBtnLayout);

    const int msgDelCol = 4;
    m_messageTable = new QTableWidget(2, 5, leftPanel);
    m_messageTable->setHorizontalHeaderLabels({"From", "To", "Label", "Arrow", "Del"});
    m_messageTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_messageTable->horizontalHeader()->setSectionResizeMode(msgDelCol, QHeaderView::Fixed);
    m_messageTable->setColumnWidth(msgDelCol, 32);
    m_messageTable->verticalHeader()->setDefaultSectionSize(28);
    leftLayout->addWidget(m_messageTable);

    auto addMessageDeleteButton = [&](int row) {
        QPushButton *delBtn = new QPushButton("\u00d7", m_messageTable);
        delBtn->setFixedSize(26, 22);
        delBtn->setToolTip("Delete row");
        m_messageTable->setCellWidget(row, msgDelCol, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, delBtn]() {
            int row = m_messageTable->indexAt(delBtn->pos()).row();
            if (row >= 0 && m_messageTable->rowCount() > 1)
                m_messageTable->removeRow(row);
            schedulePreviewUpdate();
        });
    };

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
    connect(m_participantTable, &QTableWidget::itemChanged,
            this, &MermaidSequenceDialog::onParticipantChanged);
    connect(m_messageTable, &QTableWidget::itemChanged,
            this, &MermaidSequenceDialog::schedulePreviewUpdate);

    connect(addParticipantBtn, &QPushButton::clicked, this, [this, addParticipantDeleteButton]() {
        int row = m_participantTable->rowCount();
        m_participantTable->insertRow(row);
        addParticipantDeleteButton(row);
        refreshMessageCombos();
        schedulePreviewUpdate();
    });
    connect(addMessageBtn, &QPushButton::clicked, this, [this, addMessageDeleteButton]() {
        int row = m_messageTable->rowCount();
        m_messageTable->insertRow(row);
        m_messageTable->setItem(row, 2, new QTableWidgetItem(""));
        addMessageDeleteButton(row);
        refreshMessageCombos();
        schedulePreviewUpdate();
    });

    refreshMessageCombos();

    m_messageTable->setItem(0, 2, new QTableWidgetItem("Hello"));
    QComboBox *arrow0 = qobject_cast<QComboBox*>(m_messageTable->cellWidget(0, 3));
    if (arrow0) arrow0->setCurrentIndex(0);

    m_messageTable->setItem(1, 2, new QTableWidgetItem("Hi"));
    QComboBox *arrow1 = qobject_cast<QComboBox*>(m_messageTable->cellWidget(1, 3));
    if (arrow1) arrow1->setCurrentIndex(1);
}

void MermaidSequenceDialog::onParticipantChanged()
{
    refreshMessageCombos();
    schedulePreviewUpdate();
}

void MermaidSequenceDialog::refreshMessageCombos()
{
    QStringList names;
    for (int r = 0; r < m_participantTable->rowCount(); ++r) {
        QTableWidgetItem *item = m_participantTable->item(r, 0);
        if (item && !item->text().trimmed().isEmpty())
            names.append(item->text().trimmed());
    }
    for (int r = 0; r < m_messageTable->rowCount(); ++r) {
        for (int col : {0, 1}) {
            QComboBox *box = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, col));
            if (!box) {
                box = new QComboBox(m_messageTable);
                m_messageTable->setCellWidget(r, col, box);
                connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MermaidSequenceDialog::schedulePreviewUpdate);
            }
            QString cur = box->currentText();
            box->blockSignals(true);
            box->clear();
            box->addItems(names);
            int idx = box->findText(cur);
            if (idx >= 0) box->setCurrentIndex(idx);
            box->blockSignals(false);
        }
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 3));
        if (!arrowBox) {
            arrowBox = new QComboBox(m_messageTable);
            arrowBox->addItems({"->>", "-->>", "-x", "--)", "->", "-->"});
            m_messageTable->setCellWidget(r, 3, arrowBox);
            connect(arrowBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MermaidSequenceDialog::schedulePreviewUpdate);
        }
    }
}

void MermaidSequenceDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidSequenceDialog::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped));
}

QString MermaidSequenceDialog::generatedDiagram() const
{
    return buildDiagram();
}

QString MermaidSequenceDialog::buildDiagram() const
{
    QString out = "sequenceDiagram\n";

    for (int r = 0; r < m_participantTable->rowCount(); ++r) {
        QTableWidgetItem *nameItem = m_participantTable->item(r, 0);
        QTableWidgetItem *aliasItem = m_participantTable->item(r, 1);
        QString name = nameItem ? nameItem->text().trimmed() : QString();
        QString alias = aliasItem ? aliasItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;
        out += "    participant " + name;
        if (!alias.isEmpty())
            out += " as " + alias;
        out += "\n";
    }

    for (int r = 0; r < m_messageTable->rowCount(); ++r) {
        QComboBox *fromBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 0));
        QComboBox *toBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 1));
        QTableWidgetItem *labelItem = m_messageTable->item(r, 2);
        QComboBox *arrowBox = qobject_cast<QComboBox*>(m_messageTable->cellWidget(r, 3));

        QString from = fromBox ? fromBox->currentText() : QString();
        QString to = toBox ? toBox->currentText() : QString();
        QString label = labelItem ? labelItem->text().trimmed() : QString();
        QString arrow = arrowBox ? arrowBox->currentText() : "->>";

        if (from.isEmpty() || to.isEmpty()) continue;

        out += "    " + from + arrow + to + ": " + label + "\n";
    }

    return out;
}
