#include "MermaidDialogBase.h"
#include "Preview.h"
#include "CssUtils.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QWebEngineView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QIcon>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidget>

MermaidDialogBase::MermaidDialogBase(const QString &title, const QString &themeCss,
                                     QWidget *parent)
    : QDialog(parent)
    , m_mermaidTheme(CssUtils::isDarkTheme(themeCss) ? QStringLiteral("dark")
                                                     : QStringLiteral("default"))
{
    auto colors = CssUtils::themeColors(themeCss);
    m_bgColor = colors.background.name();
    m_iconColor = colors.text;
    m_widthSpin = nullptr;

    setWindowTitle(title);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &MermaidDialogBase::updatePreview);
}

MermaidDialogBase::~MermaidDialogBase()
{
    m_previewTimer->stop();
    m_previewTimer->disconnect();
}

QString MermaidDialogBase::generatedDiagram() const
{
    return buildDiagram();
}

QString MermaidDialogBase::mermaidBlock() const
{
    QString diagram = generatedDiagram();
    if (diagram.isEmpty())
        return {};
    int w = m_widthCheck && m_widthCheck->isChecked() && m_widthSpin ? m_widthSpin->value() : 0;
    if (w > 0)
        return QStringLiteral("\n<div style=\"max-width:%1px\">\n\n```mermaid\n%2\n```\n\n</div>\n")
            .arg(w)
            .arg(diagram);
    return QStringLiteral("\n```mermaid\n%1\n```\n").arg(diagram);
}

QString MermaidDialogBase::mermaidTheme() const
{
    return m_mermaidTheme;
}

QString MermaidDialogBase::mermaidPreviewHtml(const QString &escaped, const QString &theme,
                                               const QString &bgColor)
{
    return QString(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>body{margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh;font-family:sans-serif;background-color:%3;}"
        ".error{color:#d32f2f;padding:16px;}</style>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "</head><body><div class=\"mermaid\">%1</div>"
        "<script>mermaid.initialize({startOnLoad:false,theme:'%2'});"
        "try{mermaid.run({querySelector:'.mermaid'}).catch(function(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';});"
        "}catch(e){document.body.innerHTML='<div class=\"error\">'+e+'</div>';}</script></body></html>"
    ).arg(escaped, theme, bgColor);
}

void MermaidDialogBase::setupMainLayout(QWidget *leftPanel, QVBoxLayout *leftLayout,
                                         const QList<int> &sizes)
{
    Q_UNUSED(leftLayout)
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *widthRow = new QHBoxLayout;
    m_widthCheck = new QCheckBox(tr("Set max width:"), rightWidget);
    m_widthSpin = new QSpinBox(rightWidget);
    m_widthSpin->setRange(100, 2000);
    m_widthSpin->setValue(500);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    m_widthSpin->setEnabled(false);
    connect(m_widthCheck, &QCheckBox::toggled, m_widthSpin, &QWidget::setEnabled);
    widthRow->addWidget(m_widthCheck);
    widthRow->addWidget(m_widthSpin);
    widthRow->addStretch();
    rightLayout->addLayout(widthRow);

    m_preview = new QWebEngineView(rightWidget);
    m_preview->setPage(new PreviewPage(m_preview));
    rightLayout->addWidget(m_preview, 1);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    if (!sizes.isEmpty())
        splitter->setSizes(sizes);

    mainLayout->addWidget(splitter);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    QPushButton *copyBtn = buttonBox->addButton(tr("&Copy"), QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton(tr("&Insert"), QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedDiagram());
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(900, 550);
}

void MermaidDialogBase::addDeleteButton(QTableWidget *table, int column, int row,
                                        std::function<void()> onDelete)
{
    QPushButton *delBtn = new QPushButton(themedIcon(":/icons/trash.svg", iconColor(), 16), "", table);
    delBtn->setFixedSize(26, 22);
    delBtn->setToolTip("Delete row");
    table->setCellWidget(row, column, delBtn);
    connect(delBtn, &QPushButton::clicked, this, [this, table, delBtn, onDelete = std::move(onDelete)]() {
        int row = table->indexAt(delBtn->pos()).row();
        if (row >= 0 && table->rowCount() > 1) {
            table->removeRow(row);
            if (onDelete)
                onDelete();
        }
        schedulePreviewUpdate();
    });
}

void MermaidDialogBase::populateComboColumns(QTableWidget *table,
                                              const QList<int> &columns,
                                              const QStringList &items)
{
    for (int r = 0; r < table->rowCount(); ++r) {
        for (int col : columns) {
            QComboBox *box = qobject_cast<QComboBox*>(table->cellWidget(r, col));
            if (!box) {
                box = new QComboBox(table);
                table->setCellWidget(r, col, box);
                connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MermaidDialogBase::schedulePreviewUpdate);
            }
            QString cur = box->currentText();
            box->blockSignals(true);
            box->clear();
            box->addItems(items);
            int idx = box->findText(cur);
            if (idx >= 0)
                box->setCurrentIndex(idx);
            box->blockSignals(false);
        }
    }
}

void MermaidDialogBase::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

void MermaidDialogBase::updatePreview()
{
    QString diagram = buildDiagram();
    QString escaped = diagram;
    escaped.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    m_preview->setHtml(mermaidPreviewHtml(escaped, m_mermaidTheme, m_bgColor));
}
