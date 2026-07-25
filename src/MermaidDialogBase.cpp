#include "MermaidDialogBase.h"
#include "Preview.h"
#include "CssUtils.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QWebEngineView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QTimer>
#include <QIcon>

MermaidDialogBase::MermaidDialogBase(const QString &title, const QString &themeCss,
                                     QWidget *parent)
    : QDialog(parent)
    , m_mermaidTheme(CssUtils::isDarkTheme(themeCss) ? QStringLiteral("dark")
                                                     : QStringLiteral("default"))
{
    auto colors = CssUtils::themeColors(themeCss);
    m_bgColor = colors.background.name();
    m_iconColor = colors.text;

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

    m_preview = new QWebEngineView(this);
    m_preview->setPage(new PreviewPage(m_preview));

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    if (!sizes.isEmpty())
        splitter->setSizes(sizes);

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

    resize(900, 550);
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
