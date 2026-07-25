#include "MermaidQuadrantDialog.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QGridLayout>
#include <QLineEdit>

MermaidQuadrantDialog::MermaidQuadrantDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase("Mermaid Quadrant Chart", themeCss, parent)
{
    setupUi();
    updatePreview();
    schedulePreviewUpdate();
}

void MermaidQuadrantDialog::setupUi()
{
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
        QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", iconColor(), 16), "", m_table);
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

    setupMainLayout(leftPanel, leftLayout, {420, 480});

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
