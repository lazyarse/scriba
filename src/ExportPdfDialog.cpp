#include "ExportPdfDialog.h"
#include "CssEditorDialog.h"
#include "CssLoader.h"
#include "Preview.h"
#include "Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWebEngineView>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QLabel>
#include <QFileDialog>
#include <QSplitter>
#include <QFile>
#include <QUrl>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QIcon>
#include <QSettings>
#include <QPageLayout>
#include <QPageSize>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QTimer>
#include <QDateTime>
#include <QDate>

Q_LOGGING_CATEGORY(lcPdf, "scriba.pdf", QtWarningMsg)
#include <QTime>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QSharedPointer>
#include <QMimeDatabase>

static QString replaceQrcUrls(const QString &html)
{
    QMimeDatabase mimeDb;
    static const QRegularExpression re(QStringLiteral("qrc:///[^\"' )]+"),
                                       QRegularExpression::CaseInsensitiveOption);
    QString result = html;
    int offset = 0;
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        auto match = it.next();
        QString qrcUrl = match.captured(0);
        // qrc:///twemoji/svg/1f600.svg -> :/twemoji/svg/1f600.svg
        QString resPath = QStringLiteral(":") + qrcUrl.mid(6);
        QFile f(resPath);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QByteArray data = f.readAll();
        QString mime = mimeDb.mimeTypeForFileNameAndData(qrcUrl, data).name();
        QString dataUri = QStringLiteral("data:%1;base64,%2")
                              .arg(mime, QString::fromLatin1(data.toBase64()));
        result.replace(match.capturedStart(0) + offset, match.capturedLength(0), dataUri);
        offset += dataUri.size() - match.capturedLength(0);
    }
    return result;
}

enum class CssUnit { Mm, Cm, In, Pt, Px, Pc };

static CssUnit parseCssUnit(const QString &u)
{
    if (u == QStringLiteral("mm")) return CssUnit::Mm;
    if (u == QStringLiteral("cm")) return CssUnit::Cm;
    if (u == QStringLiteral("in")) return CssUnit::In;
    if (u == QStringLiteral("pt")) return CssUnit::Pt;
    if (u == QStringLiteral("px")) return CssUnit::Px;
    if (u == QStringLiteral("pc")) return CssUnit::Pc;
    return CssUnit::Px;
}

enum class PageOrientation { Portrait, Landscape };

enum class NamedPageSize { A3, A4, A5, Letter, Legal, Tabloid, Ledger, B4, B5 };

static QSizeF namedPageSizeToSize(const QString &name)
{
    QString n = name.trimmed().toLower();
    if (n == QLatin1String("a3")) return QPageSize(QPageSize::A3).size(QPageSize::Point);
    if (n == QLatin1String("a4")) return QPageSize(QPageSize::A4).size(QPageSize::Point);
    if (n == QLatin1String("a5")) return QPageSize(QPageSize::A5).size(QPageSize::Point);
    if (n == QLatin1String("letter")) return QPageSize(QPageSize::Letter).size(QPageSize::Point);
    if (n == QLatin1String("legal")) return QPageSize(QPageSize::Legal).size(QPageSize::Point);
    if (n == QLatin1String("tabloid")) return QPageSize(QPageSize::Tabloid).size(QPageSize::Point);
    if (n == QLatin1String("ledger")) return QPageSize(QPageSize::Tabloid).size(QPageSize::Point);
    if (n == QLatin1String("b4")) return QPageSize(QPageSize::B4).size(QPageSize::Point);
    if (n == QLatin1String("b5")) return QPageSize(QPageSize::B5).size(QPageSize::Point);
    return {};
}

enum class MarginBox { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight };

static QString marginBoxToString(MarginBox box)
{
    switch (box) {
        case MarginBox::TopLeft: return QStringLiteral("top-left");
        case MarginBox::TopCenter: return QStringLiteral("top-center");
        case MarginBox::TopRight: return QStringLiteral("top-right");
        case MarginBox::BottomLeft: return QStringLiteral("bottom-left");
        case MarginBox::BottomCenter: return QStringLiteral("bottom-center");
        case MarginBox::BottomRight: return QStringLiteral("bottom-right");
    }
    return {};
}

static qreal cssLengthToPt(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^(-?\\d+(?:\\.\\d+)?)\\s*(mm|cm|in|pt|px|pc)?$"));
    auto m = re.match(s.trimmed());
    if (!m.hasMatch()) return 0;
    qreal v = m.captured(1).toDouble();
    QString u = m.captured(2);
    switch (parseCssUnit(u)) {
        case CssUnit::Mm: return v * 72.0 / 25.4;
        case CssUnit::Cm: return v * 72.0 / 2.54;
        case CssUnit::In: return v * 72.0;
        case CssUnit::Pt: return v;
        case CssUnit::Pc: return v * 12.0;
        case CssUnit::Px: return v * 0.75;
    }
    return v * 0.75;
}

static QSizeF doParsePageSize(const QString &css)
{
    static const QRegularExpression pageRe(QStringLiteral("@page\\s*\\{([^}]*)\\}"),
                                           QRegularExpression::CaseInsensitiveOption);
    auto pageMatch = pageRe.match(css);
    if (!pageMatch.hasMatch())
        return QSizeF(595.0, 842.0);

    QString block = pageMatch.captured(1);
    QRegularExpression sizeRe(QStringLiteral("size\\s*:\\s*([^;}]+)"),
                              QRegularExpression::CaseInsensitiveOption);
    auto sizeMatch = sizeRe.match(block);
    if (!sizeMatch.hasMatch())
        return QSizeF(595.0, 842.0);

    QString raw = sizeMatch.captured(1).trimmed();

    PageOrientation orientation = PageOrientation::Portrait;
    QString cleaned = raw;
    if (cleaned.contains(QStringLiteral("landscape"), Qt::CaseInsensitive)) {
        orientation = PageOrientation::Landscape;
        cleaned.remove(QStringLiteral("landscape"), Qt::CaseInsensitive);
    } else if (cleaned.contains(QStringLiteral("portrait"), Qt::CaseInsensitive)) {
        cleaned.remove(QStringLiteral("portrait"), Qt::CaseInsensitive);
    }
    cleaned = cleaned.trimmed();

    QSizeF sz = namedPageSizeToSize(cleaned);
    if (sz.isValid()) {
        if (orientation == PageOrientation::Landscape)
            sz.transpose();
        return sz;
    }

    QStringList parts = cleaned.split(QRegularExpression(QStringLiteral("\\s+")),
                                      Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        double w = cssLengthToPt(parts[0]);
        double h = cssLengthToPt(parts[1]);
        if (w > 0 && h > 0) {
            if (orientation == PageOrientation::Landscape)
                qSwap(w, h);
            return QSizeF(w, h);
        }
    }

    return QSizeF(595.0, 842.0);
}

QSizeF ExportPdfDialog::parsePageSize(const QString &css)
{
    return doParsePageSize(css);
}

static const char *mermaidInitJs = R"(
function initMermaid(){
var els=document.querySelectorAll('code.language-mermaid');
if(!els.length)return;
els.forEach(function(el){
var div=document.createElement('div');
div.className='mermaid';
div.textContent=el.textContent;
el.parentElement.parentElement.replaceChild(div,el.parentElement);
});
mermaid.run({querySelector:'.mermaid'});
}
)";

static const char *headingIdJs = R"(
function generateHeadingIds(){
document.querySelectorAll('h1,h2,h3,h4,h5,h6').forEach(function(h){
if(!h.id){
h.id=h.textContent.toLowerCase().replace(/[^\w\s-]/g,'').replace(/\s+/g,'-').replace(/^-+|-+$/g,'');
}
});
}
)";

static const char *katexInitJs = R"(
function initKaTeX(){
if(typeof renderMathInElement==='function')
renderMathInElement(document.body,{
delimiters:[
{left:'$$',right:'$$',display:true},
{left:'$',right:'$',display:false}
]});
}
)";

static const char *vegaLiteInitJs = R"(
function initVegaLite(){
var els=document.querySelectorAll('code.language-vl');
if(!els.length)return Promise.resolve();
return Promise.all(Array.from(els).map(function(el){
try{
var spec=JSON.parse(el.textContent);
var container=el.parentElement;
var div=document.createElement('div');
div.className='vega-lite-chart';
div.style.width='100%';
div.style.minHeight='300px';
div.style.overflow='visible';
container.parentElement.replaceChild(div,container);
return vegaEmbed(div,spec,{actions:false}).catch(function(){});
}
catch(e){return Promise.resolve();}
}));
}
)";

QMarginsF ExportPdfDialog::parsePageMargins(const QString &css)
{
    static const QRegularExpression pageRe(QStringLiteral("@page\\s*\\{([^}]*)\\}"),
                                           QRegularExpression::CaseInsensitiveOption);
    auto pageMatch = pageRe.match(css);
    if (!pageMatch.hasMatch())
        return QMarginsF();

    QString block = pageMatch.captured(1);
    QRegularExpression marginRe(QStringLiteral("margin\\s*:\\s*([^;]+)\\s*;"),
                                QRegularExpression::CaseInsensitiveOption);
    auto marginMatch = marginRe.match(block);
    if (!marginMatch.hasMatch())
        return QMarginsF();

    QStringList parts = marginMatch.captured(1).trimmed().split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);

    double t, r, b, l;
    if (parts.size() == 1) { t = r = b = l = cssLengthToPt(parts[0]); }
    else if (parts.size() == 2) { t = b = cssLengthToPt(parts[0]); l = r = cssLengthToPt(parts[1]); }
    else if (parts.size() == 3) { t = cssLengthToPt(parts[0]); l = r = cssLengthToPt(parts[1]); b = cssLengthToPt(parts[2]); }
    else if (parts.size() >= 4) { t = cssLengthToPt(parts[0]); r = cssLengthToPt(parts[1]); b = cssLengthToPt(parts[2]); l = cssLengthToPt(parts[3]); }
    else return QMarginsF();
    return QMarginsF(l, t, r, b);
}

QString ExportPdfDialog::findChromiumBinary()
{
    QStringList candidates = {
        QStringLiteral("/usr/bin/chromium"),
        QStringLiteral("/usr/bin/chromium-browser"),
        QStringLiteral("/usr/bin/google-chrome"),
        QStringLiteral("/usr/bin/google-chrome-stable"),
    };
    for (const auto &path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    // Also search PATH
    return QStandardPaths::findExecutable(QStringLiteral("chromium"));
}

ExportPdfDialog::ExportPdfDialog(const QString &html, const QString &defaultFilePath,
                                 CssLoader *loader, QWidget *parent)
    : QDialog(parent)
    , m_loader(loader)
    , m_html(html)
    , m_defaultFilePath(defaultFilePath)
{
    setWindowTitle("Export PDF");
    resize(1100, 700);

    setupUi();

    m_chromiumBinary = findChromiumBinary();
    qCDebug(lcPdf, "chromium binary: %s",
            m_chromiumBinary.isEmpty() ? "(none)" : qPrintable(m_chromiumBinary));

    m_hiddenEngine = new QWebEngineView(this);
    m_hiddenEngine->setFixedSize(1, 1);
    m_hiddenEngine->move(-2000, -2000);
    m_hiddenEngine->settings()->setAttribute(QWebEngineSettings::PreferCSSMarginsForPrinting, true);
    connect(m_hiddenEngine, &QWebEngineView::loadFinished, this, &ExportPdfDialog::onPageLoaded);

    m_pdfProcess = new QProcess(this);
    m_pdfProcess->setStandardOutputFile(QProcess::nullDevice());
    m_pdfProcess->setStandardErrorFile(QProcess::nullDevice());

    m_defaultRadio->setChecked(true);
    m_customCssPath.clear();
    m_showHeader->setChecked(QSettings().value(Preferences::PdfShowHeader, false).toBool());
    onCssModeChanged();
}

ExportPdfDialog::~ExportPdfDialog() = default;

void ExportPdfDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    m_showPdfToolbar = new QCheckBox("Show PDF toolbar", leftPanel);
    m_showPdfToolbar->setChecked(false);
    m_showPdfToolbar->setEnabled(false);
    connect(m_showPdfToolbar, &QCheckBox::toggled, this, &ExportPdfDialog::reloadPdfPreview);
    leftLayout->addWidget(m_showPdfToolbar);

    auto *exportGroup = new QGroupBox(QStringLiteral("Export options"), leftPanel);
    auto *groupLayout = new QVBoxLayout(exportGroup);

    groupLayout->addWidget(new QLabel("Print Stylesheet:", exportGroup));

    m_defaultRadio = new QRadioButton("Use default print stylesheet", exportGroup);
    m_customRadio = new QRadioButton("Use custom print stylesheet", exportGroup);
    groupLayout->addWidget(m_defaultRadio);

    auto *editPrintCssLayout = new QHBoxLayout();
    editPrintCssLayout->addSpacing(24);
    m_editPrintCssBtn = new QPushButton("Edit Print Base CSS...", exportGroup);
    editPrintCssLayout->addWidget(m_editPrintCssBtn);
    groupLayout->addLayout(editPrintCssLayout);
    connect(m_editPrintCssBtn, &QPushButton::clicked, this, &ExportPdfDialog::editPrintBaseCss);

    groupLayout->addWidget(m_customRadio);

    QHBoxLayout *customLayout = new QHBoxLayout();
    customLayout->addSpacing(24);
    m_pathLabel = new QLabel("No file selected", exportGroup);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet("color: gray;");
    customLayout->addWidget(m_pathLabel, 1);
    m_browseBtn = new QPushButton("Browse...", exportGroup);
    customLayout->addWidget(m_browseBtn);
    groupLayout->addLayout(customLayout);

    m_showHeader = new QCheckBox("Show headers & footers", exportGroup);
    m_showHeader->setChecked(false);
    connect(m_showHeader, &QCheckBox::toggled, this, [this]() { onCssModeChanged(); });
    groupLayout->addWidget(m_showHeader);

    auto *fieldGrid = new QGridLayout();
    fieldGrid->setHorizontalSpacing(6);

    auto *colLeftLabel = new QLabel("Left", exportGroup);
    colLeftLabel->setAlignment(Qt::AlignCenter);
    colLeftLabel->setStyleSheet("color: #888; font-size: 9px;");
    auto *colCenterLabel = new QLabel("Center", exportGroup);
    colCenterLabel->setAlignment(Qt::AlignCenter);
    colCenterLabel->setStyleSheet("color: #888; font-size: 9px;");
    auto *colRightLabel = new QLabel("Right", exportGroup);
    colRightLabel->setAlignment(Qt::AlignCenter);
    colRightLabel->setStyleSheet("color: #888; font-size: 9px;");
    fieldGrid->addWidget(colLeftLabel, 0, 1);
    fieldGrid->addWidget(colCenterLabel, 0, 2);
    fieldGrid->addWidget(colRightLabel, 0, 3);

    auto *headerRowLabel = new QLabel("Header:", exportGroup);
    headerRowLabel->setEnabled(false);
    fieldGrid->addWidget(headerRowLabel, 1, 0);

    m_headerLeft = new QPlainTextEdit(exportGroup);
    m_headerLeft->setPlaceholderText("{date}");
    m_headerLeft->setMaximumHeight(60);
    m_headerLeft->setEnabled(false);
    fieldGrid->addWidget(m_headerLeft, 1, 1);

    m_headerCenter = new QPlainTextEdit(exportGroup);
    m_headerCenter->setPlaceholderText("{title}");
    m_headerCenter->setMaximumHeight(60);
    m_headerCenter->setEnabled(false);
    fieldGrid->addWidget(m_headerCenter, 1, 2);

    m_headerRight = new QPlainTextEdit(exportGroup);
    m_headerRight->setPlaceholderText("Page {page}");
    m_headerRight->setMaximumHeight(60);
    m_headerRight->setEnabled(false);
    fieldGrid->addWidget(m_headerRight, 1, 3);

    auto *footerRowLabel = new QLabel("Footer:", exportGroup);
    footerRowLabel->setEnabled(false);
    fieldGrid->addWidget(footerRowLabel, 2, 0);

    m_footerLeft = new QPlainTextEdit(exportGroup);
    m_footerLeft->setPlaceholderText("{date}");
    m_footerLeft->setMaximumHeight(60);
    m_footerLeft->setEnabled(false);
    fieldGrid->addWidget(m_footerLeft, 2, 1);

    m_footerCenter = new QPlainTextEdit(exportGroup);
    m_footerCenter->setPlaceholderText("{title}");
    m_footerCenter->setMaximumHeight(60);
    m_footerCenter->setEnabled(false);
    fieldGrid->addWidget(m_footerCenter, 2, 2);

    m_footerRight = new QPlainTextEdit(exportGroup);
    m_footerRight->setPlaceholderText("Page {page}");
    m_footerRight->setMaximumHeight(60);
    m_footerRight->setEnabled(false);
    fieldGrid->addWidget(m_footerRight, 2, 3);

    fieldGrid->setColumnStretch(1, 1);
    fieldGrid->setColumnStretch(2, 1);
    fieldGrid->setColumnStretch(3, 1);
    groupLayout->addLayout(fieldGrid);

    auto enableFields = [=](bool on) {
        m_headerLeft->setEnabled(on);
        m_headerCenter->setEnabled(on);
        m_headerRight->setEnabled(on);
        m_footerLeft->setEnabled(on);
        m_footerCenter->setEnabled(on);
        m_footerRight->setEnabled(on);
        colLeftLabel->setEnabled(on);
        colCenterLabel->setEnabled(on);
        colRightLabel->setEnabled(on);
        headerRowLabel->setEnabled(on);
        footerRowLabel->setEnabled(on);
    };
    connect(m_showHeader, &QCheckBox::toggled, this, enableFields);

    auto *hintLabel = new QLabel(
        QStringLiteral("Variables: <code>{page}</code> <code>{pages}</code> "
                       "<code>{date}</code> <code>{time}</code> <code>{title}</code>"),
        exportGroup);
    hintLabel->setEnabled(false);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #888; font-size: 10px;");
    connect(m_showHeader, &QCheckBox::toggled, hintLabel, &QWidget::setEnabled);
    groupLayout->addWidget(hintLabel);

    m_regenerateBtn = new QPushButton("Regenerate preview", exportGroup);
    m_regenerateBtn->setEnabled(false);
    connect(m_showHeader, &QCheckBox::toggled, m_regenerateBtn, &QWidget::setEnabled);
    connect(m_regenerateBtn, &QPushButton::clicked, this, &ExportPdfDialog::onCssModeChanged);
    groupLayout->addWidget(m_regenerateBtn);

    leftLayout->addWidget(exportGroup);
    leftLayout->addStretch();

    m_preview = new QWebEngineView(this);
    m_preview->setPage(new PreviewPage(m_preview));
    m_preview->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    m_preview->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);


    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 780});
    splitter->handle(1)->setEnabled(false);

    mainLayout->addWidget(splitter);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Save, this);
    buttonBox->button(QDialogButtonBox::Save)->setText("Export PDF");
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(m_defaultRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_customRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportPdfDialog::browseCustomCss);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() { accept(); });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ExportPdfDialog::onCssModeChanged()
{
    bool custom = m_customRadio->isChecked();
    m_browseBtn->setEnabled(custom);

    m_currentPrintCss = loadCustomCss();
    if (m_currentPrintCss.isEmpty())
        m_currentPrintCss = m_loader->printCss();

    if (m_showHeader->isChecked())
        m_currentPrintCss += buildHeaderFooterCss();

    {
        QSizeF pagePt = parsePageSize(m_currentPrintCss);
        QMarginsF marginsPt = parsePageMargins(m_currentPrintCss);
        double cwPt = pagePt.width() - marginsPt.left() - marginsPt.right();
        int cwPx = static_cast<int>(cwPt * 96.0 / 72.0 + 0.5);
        m_hiddenEngine->setFixedSize(std::max(400, cwPx), 600);
        m_hiddenEngine->move(-2000, -2000);
    }

    QSettings s;
    s.setValue(Preferences::PdfShowHeader, m_showHeader->isChecked());

    if (custom && !m_customCssPath.isEmpty())
        m_pathLabel->setText(m_customCssPath);
    else
        m_pathLabel->setText("No file selected");

    m_currentFullHtml = buildFullHtml(m_currentPrintCss);
    m_baseUrl = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../").toString();
    m_preview->setHtml(QStringLiteral(
        "<!DOCTYPE html><html><body style=\"display:flex;justify-content:center;align-items:center;height:100vh;margin:0;font-family:sans-serif;color:#999;font-size:14pt;\">"
        "<p>Generating PDF\u2026</p></body></html>"
    ));
    m_pdfData.clear();
    ++m_generationId;
    m_hiddenEngine->setHtml(m_currentFullHtml, QUrl(m_baseUrl));
}

void ExportPdfDialog::browseCustomCss()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Select Print CSS File", QString(), "CSS Files (*.css)");
    if (path.isEmpty()) return;

    m_customCssPath = path;
    onCssModeChanged();
}

static QString loadResourceCss(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return {};
}

void ExportPdfDialog::editPrintBaseCss()
{
    CssEditorDialog dlg("Edit Print Base CSS", m_loader->printBaseCss(),
        loadResourceCss(":/print-base.css"), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_loader->setPrintBaseCss(dlg.css());
        onCssModeChanged();
    }
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

QString ExportPdfDialog::buildFullHtml(const QString &printCss) const
{
    QSettings settings;
    bool striping = settings.value(Preferences::TableStriping, true).toBool();
    QString stripeCss = striping ? QString()
        : QLatin1String(Preferences::TableStripePdfCss);

    QString emojiMode = settings.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    return QString(
        "<!DOCTYPE html><html><head>"
        "<style>%1</style>"
        "<style>%2</style>"
        "<style>#preview .emoji-char{font-family:'Symbola',monospace}.emoji{height:1em;width:1em;vertical-align:-0.1em;display:inline-block}</style>"
        "<script src=\"qrc:///highlight.min.js\"></script>"
        "<script src=\"qrc:///mermaid.min.js\"></script>"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/auto-render.min.js\"></script>"
        "<script src=\"qrc:///vega.min.js\"></script>"
        "<script src=\"qrc:///vega-lite.min.js\"></script>"
        "<script src=\"qrc:///vega-embed.min.js\"></script>"
        "<script src=\"qrc:///twemoji.min.js\"></script>"
        "<script src=\"qrc:///emoji.js\"></script>"
        "<script>%3%4%5%6"
        "function twemojiParse(m){if(m==='color'&&typeof twemoji!=='undefined'){twemoji.parse(document.body,{base:'qrc:///twemoji/',folder:'svg',ext:'.svg',className:'emoji'});}}"
        "document.addEventListener('DOMContentLoaded',function(){"
        "mermaid.initialize({startOnLoad:false,theme:'default'});"
        "initMermaid();hljs.highlightAll();generateHeadingIds();initKaTeX();window.vegaLiteReady=initVegaLite();"
        "replaceEmoji(document.body);twemojiParse('%7');"
        "});</script>"
        "</head><body id=\"preview\">%8</body></html>"
    ).arg(printCss, stripeCss,
          mermaidInitJs, headingIdJs, katexInitJs, vegaLiteInitJs,
          emojiMode, m_html);
}

void ExportPdfDialog::onPageLoaded(bool ok)
{
    if (!ok) return;

    int genId = m_generationId;

    // Wait for async Vega rendering (vegaEmbed returns a Promise stored on
    // window.vegaLiteReady).  If there are no vega charts the promise resolves
    // immediately so this is a no-op for documents without vega.
    QString css = m_currentPrintCss;
    m_hiddenEngine->page()->runJavaScript(
        QStringLiteral("(window.vegaLiteReady||Promise.resolve()).then(function(){return true;})"),
        [this, genId, css](const QVariant &) {
            if (genId != m_generationId) return;

            if (m_chromiumBinary.isEmpty()) {
                qCDebug(lcPdf, "no chromium binary found, using Qt printToPdf (headers cannot be suppressed)");
                QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                   QMarginsF(), QPageLayout::Point);
                m_hiddenEngine->page()->printToPdf([this, genId](const QByteArray &data) {
                    if (genId != m_generationId) return;
                    m_pdfData = data;
                    m_tempFile.reset(new QTemporaryFile());
                    if (m_tempFile->open()) {
                        m_tempFile->write(data);
                        m_tempFile->flush();
                        m_preview->load(QUrl::fromLocalFile(m_tempFile->fileName()));
                    }
                }, layout);
            } else {
                generatePdfViaChromium(css);
            }
        }
    );
}

void ExportPdfDialog::generatePdfViaChromium(const QString &printCss)
{
    int genId = m_generationId;

    // Fix Vega SVGs: vega-embed uses width="100%" (CSS-relative).
    // Chromium headless --print-to-pdf renders such SVGs at zero width
    // (bug #40712208). Bake explicit pixel dimensions from viewBox before
    // extraction.  The viewBox always carries intrinsic dimensions and
    // doesn't depend on widget layout (which may be stale for hidden views).
    // Mermaid SVGs already have explicit attributes and are unaffected.
    QString js = QStringLiteral(
        "(function() {"
        "  document.querySelectorAll('.vega-lite-chart svg').forEach(function(svg) {"
        "    var vb = svg.getAttribute('viewBox');"
        "    if (vb) {"
        "      var parts = vb.split(/\\s+/);"
        "      if (parts.length === 4) {"
        "        svg.setAttribute('width', parts[2]);"
        "        svg.setAttribute('height', parts[3]);"
        "      }"
        "    }"
        "  });"
        "  var s = '';"
        "  var els = document.querySelectorAll('style');"
        "  for (var i = 0; i < els.length; i++) s += els[i].outerHTML;"
        "  return s + document.body.outerHTML;"
        "})()"
    );

    m_hiddenEngine->page()->runJavaScript(js, [this, genId](const QVariant &result) {
        if (genId != m_generationId) return;

        QString bodyHtml = result.toString();
        if (bodyHtml.isEmpty()) {
            qCDebug(lcPdf, "extracted body HTML is empty!");
            return;
        }
        qCDebug(lcPdf, "extracted body HTML: %zu bytes",
                static_cast<size_t>(bodyHtml.size()));

        bodyHtml = replaceQrcUrls(bodyHtml);

        QString metaHead = QStringLiteral(
            "<meta charset=\"utf-8\">"
            "<base href=\"%1\">"
        ).arg(m_baseUrl.toHtmlEscaped());

        // bodyHtml = "<style>...</style><body>...</body>" after extraction
        // Split so <style> elements go inside <head> for valid HTML
        int bodyTagIdx = bodyHtml.indexOf(QStringLiteral("<body"));
        QString stylesBlock;
        QString bodyPart;
        if (bodyTagIdx >= 0) {
            stylesBlock = bodyHtml.left(bodyTagIdx);
            bodyPart = bodyHtml.mid(bodyTagIdx);
        } else {
            stylesBlock = bodyHtml;
            bodyPart = QStringLiteral("<body></body>");
        }

        QString fullDoc = QStringLiteral(
            "<!DOCTYPE html>\n<html><head>%1%2</head>%3</html>\n"
        ).arg(metaHead, stylesBlock, bodyPart);

        auto dir = QSharedPointer<QTemporaryDir>::create();
        if (!dir->isValid()) return;
        QString htmlPath = dir->filePath(QStringLiteral("input.html"));
        QString pdfPath = dir->filePath(QStringLiteral("output.pdf"));

        QFile htmlFile(htmlPath);
        if (!htmlFile.open(QIODevice::WriteOnly)) return;
        htmlFile.write(fullDoc.toUtf8());
        htmlFile.close();

        QStringList args;
        args << QStringLiteral("--headless=new")
             << QStringLiteral("--no-margins")
             << QStringLiteral("--no-pdf-header-footer")
             << QStringLiteral("--print-to-pdf=%1").arg(pdfPath)
             << QUrl::fromLocalFile(htmlPath).toString();

        auto onFinished = [this, genId, dir, pdfPath](int exitCode, QProcess::ExitStatus) {
            if (genId != m_generationId) return;
            if (exitCode != 0) return;

            QFile f(pdfPath);
            if (!f.open(QIODevice::ReadOnly)) return;
            m_pdfData = f.readAll();
            qCDebug(lcPdf, "chromium generated: %zu bytes", m_pdfData.size());

            m_tempFile.reset(new QTemporaryFile());
            if (m_tempFile->open()) {
                m_tempFile->write(m_pdfData);
                m_tempFile->flush();
                m_pdfUrl = m_tempFile->fileName();
                m_showPdfToolbar->setEnabled(true);
                reloadPdfPreview();
            }
        };

        qCDebug(lcPdf, "using chromium: %s  args: %s",
                qPrintable(m_chromiumBinary), qPrintable(args.join(' ')));
        QObject::connect(m_pdfProcess, &QProcess::finished,
                         m_pdfProcess, onFinished, Qt::SingleShotConnection);
        m_pdfProcess->start(m_chromiumBinary, args);
    });
}

static QString cssContentValue(const QString &text)
{
    QString result;
    int lastEnd = 0;
    QRegularExpression re(QStringLiteral("\\{page(s)?\\}"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        QString literal = text.mid(lastEnd, match.capturedStart() - lastEnd);
        if (!literal.isEmpty())
            result += QStringLiteral("\"%1\" ").arg(literal.toHtmlEscaped());
        result += match.captured(1).isEmpty()
            ? QStringLiteral("counter(page) ")
            : QStringLiteral("counter(pages) ");
        lastEnd = match.capturedEnd();
    }
    QString literal = text.mid(lastEnd);
    if (!literal.isEmpty())
        result += QStringLiteral("\"%1\" ").arg(literal.toHtmlEscaped());
    return result.trimmed();
}

QString ExportPdfDialog::buildHeaderFooterCss() const
{
    auto subst = [this](QString s) -> QString {
        s.replace(QStringLiteral("{date}"), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
        s.replace(QStringLiteral("{time}"), QTime::currentTime().toString(QStringLiteral("HH:mm")));
        s.replace(QStringLiteral("{title}"), QFileInfo(m_defaultFilePath).completeBaseName());
        s.replace(QStringLiteral("{url}"), QString());
        return s;
    };

    auto addField = [&](const QString &marginBox, QPlainTextEdit *field) -> QString {
        QString text = field->toPlainText().trimmed();
        if (text.isEmpty()) return {};
        return QStringLiteral("@page { @%1 { content: %2; font-size: 9pt; color: #666; } }\n")
            .arg(marginBox, cssContentValue(subst(text)));
    };

    QString css;
    css += addField(marginBoxToString(MarginBox::TopLeft), m_headerLeft);
    css += addField(marginBoxToString(MarginBox::TopCenter), m_headerCenter);
    css += addField(marginBoxToString(MarginBox::TopRight), m_headerRight);
    css += addField(marginBoxToString(MarginBox::BottomLeft), m_footerLeft);
    css += addField(marginBoxToString(MarginBox::BottomCenter), m_footerCenter);
    css += addField(marginBoxToString(MarginBox::BottomRight), m_footerRight);
    return css;
}

void ExportPdfDialog::reloadPdfPreview()
{
    if (m_pdfUrl.isEmpty()) return;
    QUrl url = QUrl::fromLocalFile(m_pdfUrl);
    url.setQuery(QStringLiteral("_=%1").arg(QDateTime::currentMSecsSinceEpoch()));
    if (!m_showPdfToolbar->isChecked())
        url.setFragment(QStringLiteral("toolbar=0&navpanes=0"));
    m_preview->load(url);
}

void ExportPdfDialog::accept()
{
    if (m_pdfData.isEmpty())
        return;

    QString defaultName = "Untitled.pdf";
    if (!m_defaultFilePath.isEmpty()) {
        QFileInfo fi(m_defaultFilePath);
        defaultName = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export PDF", defaultName, "PDF Files (*.pdf)");
    if (filePath.isEmpty()) return;

    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly))
        f.write(m_pdfData);

    QDialog::accept();
}
