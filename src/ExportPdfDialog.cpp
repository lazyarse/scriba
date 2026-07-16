#include "ExportPdfDialog.h"
#include "CssManager.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QPushButton>
#include <QWebEngineView>
#include <QSettings>
#include <QLabel>
#include <QFileDialog>
#include <QSplitter>
#include <QFile>
#include <QUrl>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QIcon>

ExportPdfDialog::ExportPdfDialog(const QString &html, CssManager *cssManager, QWidget *parent)
    : QDialog(parent)
    , m_cssManager(cssManager)
    , m_html(html)
{
    setWindowTitle("Export PDF");
    resize(1100, 700);

    setupUi();

    QSettings settings;
    m_customCssPath = settings.value("activePrintCssFile", "").toString();
    if (!m_customCssPath.isEmpty() && QFile::exists(m_customCssPath)) {
        m_customRadio->setChecked(true);
    } else {
        m_defaultRadio->setChecked(true);
        m_customCssPath.clear();
    }

    onCssModeChanged();
}

void ExportPdfDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel: CSS selection
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    leftLayout->addWidget(new QLabel("Print Stylesheet:"));

    m_defaultRadio = new QRadioButton("Use default print stylesheet", leftPanel);
    m_customRadio = new QRadioButton("Use custom print stylesheet", leftPanel);
    leftLayout->addWidget(m_defaultRadio);
    leftLayout->addWidget(m_customRadio);

    QHBoxLayout *customLayout = new QHBoxLayout();
    customLayout->addSpacing(24);
    m_pathLabel = new QLabel("No file selected", leftPanel);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet("color: gray;");
    customLayout->addWidget(m_pathLabel, 1);
    m_browseBtn = new QPushButton("Browse...", leftPanel);
    customLayout->addWidget(m_browseBtn);
    leftLayout->addLayout(customLayout);

    leftLayout->addStretch();

    // Right panel: preview
    m_preview = new QWebEngineView(this);

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 780});
    splitter->handle(1)->setEnabled(false);

    mainLayout->addWidget(splitter);

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    buttonBox->button(QDialogButtonBox::Save)->setText("Export PDF");
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(m_defaultRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_customRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportPdfDialog::browseCustomCss);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (!m_customCssPath.isEmpty()) {
            QSettings settings;
            settings.setValue("activePrintCssFile", m_customCssPath);
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ExportPdfDialog::onCssModeChanged()
{
    bool custom = m_customRadio->isChecked();
    m_browseBtn->setEnabled(custom);

    QString printCss = loadCustomCss();
    if (printCss.isEmpty())
        printCss = m_cssManager->printCss();

    if (custom && !m_customCssPath.isEmpty())
        m_pathLabel->setText(m_customCssPath);
    else
        m_pathLabel->setText("No file selected");

    loadPreview(printCss);
}

void ExportPdfDialog::browseCustomCss()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Select Print CSS File", QString(), "CSS Files (*.css)");
    if (path.isEmpty()) return;

    m_customCssPath = path;
    onCssModeChanged();
}

void ExportPdfDialog::loadPreview(const QString &printCss)
{
    QString baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    QSettings settings;
    bool striping = settings.value(Preferences::TableStriping, true).toBool();
    QString stripeCss = striping ? QString()
        : QStringLiteral("tr:nth-child(even),tr:nth-child(even) td{background-color:transparent !important}");
    QString grayscaleHljs =
        ".hljs{background:#fff;color:#1a1a1a}"
        ".hljs-comment,.hljs-quote{color:#808080;font-style:italic}"
        ".hljs-keyword,.hljs-selector-tag,.hljs-section,.hljs-title,.hljs-name{color:#333;font-weight:bold}"
        ".hljs-string,.hljs-selector-attr,.hljs-selector-pseudo,.hljs-regexp{color:#555}"
        ".hljs-literal,.hljs-symbol,.hljs-bullet,.hljs-meta,.hljs-number,.hljs-link{color:#444}"
        ".hljs-doctag,.hljs-type,.hljs-attr,.hljs-built_in,.hljs-params{color:#2a2a2a}"
        ".hljs-attribute,.hljs-subst{color:#1a1a1a}"
        ".hljs-addition{background-color:#e0e0e0;color:#1a1a1a}"
        ".hljs-deletion{background-color:#d0d0d0;color:#1a1a1a}"
        ".hljs-strong{font-weight:bold}"
        ".hljs-emphasis{font-style:italic}";
    QString fullHtml = QString(
        "<html><head>"
        "<style>%1</style>"
        "<style>%2</style>"
        "<style>%3</style>"
        "<style>%4</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script>document.addEventListener('DOMContentLoaded',function(){hljs.highlightAll();});</script>"
        "</head><body>%5</body></html>"
    ).arg(printCss, m_cssManager->previewBaseCss(), stripeCss, grayscaleHljs, m_html);
    m_preview->setHtml(fullHtml, QUrl(baseUrl));
}

QString ExportPdfDialog::selectedPrintCss() const
{
    QString css = loadCustomCss();
    return css.isEmpty() ? m_cssManager->printCss() : css;
}

QString ExportPdfDialog::loadCustomCss() const
{
    if (m_customRadio->isChecked() && !m_customCssPath.isEmpty() && QFile::exists(m_customCssPath)) {
        QFile f(m_customCssPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString::fromUtf8(f.readAll());
    }
    return {};
}
