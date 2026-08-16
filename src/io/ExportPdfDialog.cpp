// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "ExportPdfDialog.h"
#include "StaticHelpers.h"
#include "css/CssEditorDialog.h"
#include "css/CssLoader.h"
#include "css/CssUtils.h"
#include "preview/Preview.h"
#include "prefs/Preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
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
#include <QPrinter>
#include <QPrintDialog>
#include <QEventLoop>
#include <QSignalBlocker>
#include "preview/JsRenderEngine.h"

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

QSizeF ExportPdfDialog::parsePageSize(const QString &css)
{
    return PrintOptions::parsePageSize(css);
}

QMarginsF ExportPdfDialog::parsePageMargins(const QString &css)
{
    return PrintOptions::parsePageMargins(css);
}

QString ExportPdfDialog::findChromiumBinary()
{
    QStringList candidates = {
        QStringLiteral("/usr/bin/google-chrome-stable"),
        QStringLiteral("/usr/bin/google-chrome"),
        QStringLiteral("/usr/bin/chromium"),
        QStringLiteral("/usr/bin/chromium-browser"),
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
    , m_printOptions(PrintOptions::fromSettings())
{
    setWindowTitle("Print / Export PDF");
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

ExportPdfDialog::~ExportPdfDialog()
{
    // Reap a still-running chromium: QProcess's destructor does NOT kill the
    // child, so closing the dialog mid-generation would otherwise leave an
    // orphaned headless browser running (and, in tests, leaking processes
    // that pile up under parallel runs).
    if (m_pdfProcess && m_pdfProcess->state() != QProcess::NotRunning) {
        m_pdfProcess->kill();
        m_pdfProcess->waitForFinished();
    }
}

void ExportPdfDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QVBoxLayout *leftLayout = setupLayout(mainLayout);
    setupCssModeSection(leftLayout);
    setupTypesettingSection(leftLayout);
    setupConnections();

    applyPrintOptionsToUi(m_printOptions);
}

// Builds the outer chrome: the horizontal splitter hosting the options panel
// and the PDF preview, plus the bottom action bar. Returns the left panel's
// vertical layout so the section builders can add their group boxes to it.
QVBoxLayout *ExportPdfDialog::setupLayout(QVBoxLayout *mainLayout)
{
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    m_showPdfToolbar = new QCheckBox("Show PDF toolbar", leftPanel);
    m_showPdfToolbar->setChecked(false);
    m_showPdfToolbar->setEnabled(false);
    connect(m_showPdfToolbar, &QCheckBox::toggled, this, &ExportPdfDialog::reloadPdfPreview);
    leftLayout->addWidget(m_showPdfToolbar);

    m_preview = createPreviewView(this, m_loader->themeCss());
    m_preview->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    m_preview->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);


    splitter->addWidget(leftPanel);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 780});
    splitter->handle(1)->setEnabled(false);

    mainLayout->addWidget(splitter);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    auto *cancelBtn = new QPushButton(tr("&Cancel"));
    auto *exportPdfBtn = new QPushButton(tr("E&xport PDF"));
    auto *printBtn = new QPushButton(tr("&Print..."));
    stripButtonIcon(cancelBtn);
    stripButtonIcon(exportPdfBtn);
    stripButtonIcon(printBtn);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(exportPdfBtn, &QPushButton::clicked, this, [this]() { accept(); });
    connect(printBtn, &QPushButton::clicked, this, &ExportPdfDialog::printDocument);

    bottomLayout->addWidget(cancelBtn);
    bottomLayout->addWidget(exportPdfBtn);
    bottomLayout->addWidget(printBtn);
    mainLayout->addLayout(bottomLayout);

    return leftLayout;
}

void ExportPdfDialog::setupCssModeSection(QVBoxLayout *leftLayout)
{
    auto *exportGroup = new QGroupBox(QStringLiteral("Export options"), leftLayout->parentWidget());
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
    colLeftLabel->setStyleSheet("color: #888;");
    auto *colCenterLabel = new QLabel("Center", exportGroup);
    colCenterLabel->setAlignment(Qt::AlignCenter);
    colCenterLabel->setStyleSheet("color: #888;");
    auto *colRightLabel = new QLabel("Right", exportGroup);
    colRightLabel->setAlignment(Qt::AlignCenter);
    colRightLabel->setStyleSheet("color: #888;");
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

    auto enableFields = [=, this](bool on) {
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
    hintLabel->setStyleSheet("color: #888;");
    connect(m_showHeader, &QCheckBox::toggled, hintLabel, &QWidget::setEnabled);
    groupLayout->addWidget(hintLabel);

    m_regenerateBtn = new QPushButton("Regenerate preview", exportGroup);
    m_regenerateBtn->setEnabled(false);
    connect(m_showHeader, &QCheckBox::toggled, m_regenerateBtn, &QWidget::setEnabled);
    connect(m_regenerateBtn, &QPushButton::clicked, this, &ExportPdfDialog::onCssModeChanged);
    groupLayout->addWidget(m_regenerateBtn);

    leftLayout->addWidget(exportGroup);
}

void ExportPdfDialog::setupTypesettingSection(QVBoxLayout *leftLayout)
{
    auto *typesetGroup = new QGroupBox("Typesetting", this);
    auto *tsLayout = new QVBoxLayout(typesetGroup);

    auto *splitRow = new QHBoxLayout();
    auto *splitLabel = new QLabel("Split code blocks:", typesetGroup);
    m_codeSplitCombo = new QComboBox(typesetGroup);
    m_codeSplitCombo->addItem("Never", QVariant(QStringLiteral("never")));
    m_codeSplitCombo->addItem("Blocks over 50 lines", QVariant(QStringLiteral("small")));
    m_codeSplitCombo->addItem("Blocks over 100 lines", QVariant(QStringLiteral("large")));
    splitRow->addWidget(splitLabel);
    splitRow->addWidget(m_codeSplitCombo, 1);
    tsLayout->addLayout(splitRow);

    m_keepTables = new QCheckBox("Keep tables together", typesetGroup);
    m_keepHeadings = new QCheckBox("Keep headings with following text", typesetGroup);
    m_keepFigures = new QCheckBox("Keep figures and quotes together", typesetGroup);
    m_orphanControl = new QCheckBox("Avoid orphan/widow lines", typesetGroup);
    tsLayout->addWidget(m_keepTables);
    tsLayout->addWidget(m_keepHeadings);
    tsLayout->addWidget(m_keepFigures);
    tsLayout->addWidget(m_orphanControl);

    auto *geoRow = new QHBoxLayout();
    geoRow->addWidget(new QLabel("Margin:", typesetGroup));
    m_marginEdit = new QLineEdit(typesetGroup);
    m_marginEdit->setPlaceholderText("e.g. 18mm");
    geoRow->addWidget(m_marginEdit, 1);
    geoRow->addWidget(new QLabel("Page size:", typesetGroup));
    m_sizeEdit = new QLineEdit(typesetGroup);
    m_sizeEdit->setPlaceholderText("e.g. A4");
    geoRow->addWidget(m_sizeEdit, 1);
    tsLayout->addLayout(geoRow);

    auto *tsHint = new QLabel(
        "Overrides apply to this export only. Saved defaults are set in "
        "Preferences → Printing.", typesetGroup);
    tsHint->setWordWrap(true);
    tsHint->setStyleSheet("color: #888;");
    tsLayout->addWidget(tsHint);

    m_resetTypesettingBtn = new QPushButton(tr("&Reset to saved defaults"), typesetGroup);
    stripButtonIcon(m_resetTypesettingBtn);
    tsLayout->addWidget(m_resetTypesettingBtn);

    leftLayout->addWidget(typesetGroup);
    leftLayout->addStretch();
}

void ExportPdfDialog::setupConnections()
{
    connect(m_defaultRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_customRadio, &QRadioButton::toggled, this, &ExportPdfDialog::onCssModeChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportPdfDialog::browseCustomCss);

    // Typesetting group: seeded from saved defaults, changes override this
    // export only (never persisted). All controls regenerate the full HTML.
    auto regenerate = [this]() {
        syncPrintOptionsFromUi();
        onCssModeChanged();
    };
    connect(m_codeSplitCombo, &QComboBox::currentIndexChanged,
            this, [regenerate](int) { regenerate(); });
    connect(m_keepTables, &QCheckBox::toggled, this, [regenerate](bool) { regenerate(); });
    connect(m_keepHeadings, &QCheckBox::toggled, this, [regenerate](bool) { regenerate(); });
    connect(m_keepFigures, &QCheckBox::toggled, this, [regenerate](bool) { regenerate(); });
    connect(m_orphanControl, &QCheckBox::toggled, this, [regenerate](bool) { regenerate(); });
    connect(m_marginEdit, &QLineEdit::editingFinished, this, [regenerate]() { regenerate(); });
    connect(m_sizeEdit, &QLineEdit::editingFinished, this, [regenerate]() { regenerate(); });
    connect(m_resetTypesettingBtn, &QPushButton::clicked, this, [this]() {
        applyPrintOptionsToUi(PrintOptions::fromSettings());
        syncPrintOptionsFromUi();
        onCssModeChanged();
    });
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
        QSizeF pagePt = parsePageSize(buildMergedPrintCss(m_currentPrintCss));
        QMarginsF marginsPt = parsePageMargins(buildMergedPrintCss(m_currentPrintCss));
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
    m_baseUrl = m_defaultFilePath.isEmpty()
        ? QString()
        : QUrl::fromLocalFile(QFileInfo(m_defaultFilePath).absolutePath() + "/").toString();
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

void ExportPdfDialog::editPrintBaseCss()
{
    CssEditorDialog dlg("Edit Print Base CSS", m_loader->printBaseCss(),
        readResourceFile(":/print-base.css"), m_loader->themeCss(), this);
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

void ExportPdfDialog::syncPrintOptionsFromUi()
{
    const QString split = m_codeSplitCombo->currentData().toString();
    if (split == QLatin1String("small"))
        m_printOptions.codeSplit = PrintOptions::CodeSplit::SplitSmall;
    else if (split == QLatin1String("large"))
        m_printOptions.codeSplit = PrintOptions::CodeSplit::SplitLarge;
    else
        m_printOptions.codeSplit = PrintOptions::CodeSplit::NeverSplit;

    m_printOptions.keepTables = m_keepTables->isChecked();
    m_printOptions.keepHeadings = m_keepHeadings->isChecked();
    m_printOptions.keepFigures = m_keepFigures->isChecked();
    m_printOptions.orphanControl = m_orphanControl->isChecked();
    m_printOptions.pageMargin = m_marginEdit->text().trimmed();
    m_printOptions.pageSize = m_sizeEdit->text().trimmed();
}

void ExportPdfDialog::applyPrintOptionsToUi(const PrintOptions::Options &o)
{
    // Blocking signals keeps seeding from firing the regenerate path before
    // m_hiddenEngine exists (constructor) or double-regenerating (reset).
    const QSignalBlocker b1(m_codeSplitCombo);
    const QSignalBlocker b2(m_keepTables);
    const QSignalBlocker b3(m_keepHeadings);
    const QSignalBlocker b4(m_keepFigures);
    const QSignalBlocker b5(m_orphanControl);
    const QSignalBlocker b6(m_marginEdit);
    const QSignalBlocker b7(m_sizeEdit);

    QString split = QStringLiteral("never");
    switch (o.codeSplit) {
    case PrintOptions::CodeSplit::SplitSmall: split = QStringLiteral("small"); break;
    case PrintOptions::CodeSplit::SplitLarge: split = QStringLiteral("large"); break;
    case PrintOptions::CodeSplit::NeverSplit: split = QStringLiteral("never"); break;
    }
    int idx = m_codeSplitCombo->findData(QVariant(split));
    m_codeSplitCombo->setCurrentIndex(qMax(0, idx));
    m_keepTables->setChecked(o.keepTables);
    m_keepHeadings->setChecked(o.keepHeadings);
    m_keepFigures->setChecked(o.keepFigures);
    m_orphanControl->setChecked(o.orphanControl);
    m_marginEdit->setText(o.pageMargin);
    m_sizeEdit->setText(o.pageSize);
}

QString ExportPdfDialog::buildMergedPrintCss(const QString &printCss) const
{
    QSettings settings;
    bool striping = settings.value(Preferences::TableStriping, true).toBool();
    QString stripeCss = striping ? QString()
        : QLatin1String(Preferences::TableStripePdfCss);

    QString mergedCss = printCss;
    if (!stripeCss.isEmpty())
        mergedCss += QStringLiteral("\n") + stripeCss;

    bool showCodeLang = settings.value(Preferences::ShowCodeLangExport, true).toBool();
    if (!showCodeLang)
        mergedCss += QStringLiteral("\n") + QLatin1String(Preferences::HideCodeLangCss);

    // Typesetting override fragments, appended LAST so they win the cascade
    // over print-base.css (DR-2: defaults emit nothing; DR-4: the @page block
    // must be last for both the chromium path and the Qt fallback).
    const QString optionCss = PrintOptions::buildCss(m_printOptions);
    if (!optionCss.isEmpty())
        mergedCss += QStringLiteral("\n") + optionCss;
    const QString pageCss = PrintOptions::buildPageOverrideCss(m_printOptions);
    if (!pageCss.isEmpty())
        mergedCss += QStringLiteral("\n") + pageCss;

    return mergedCss;
}

QString ExportPdfDialog::buildFullHtml(const QString &printCss) const
{
    QSettings settings;
    const QString mergedCss = buildMergedPrintCss(printCss);

    QString emojiMode = settings.value(Preferences::EmojiMode,
        Preferences::emojiRenderingToString(Preferences::EmojiRendering::Bw)).toString();

    QString mermaidTheme = CssUtils::isDarkTheme(m_loader->themeCss())
        ? QStringLiteral("dark") : QStringLiteral("default");

    return JsRenderEngine::buildFullHtml(m_html, mergedCss, emojiMode, mermaidTheme);
}

void ExportPdfDialog::onPageLoaded(bool ok)
{
    if (!ok) return;

    int genId = m_generationId;

    const QString css = buildMergedPrintCss(m_currentPrintCss);

    // Prepare-print pass: in a code-split mode, tag every <pre> that is taller
    // than the page content box with the mode's split class. Base print-base.css
    // keeps all <pre> together; only tagged blocks fall back to break-inside:auto
    // and split across pages. Blocks that fit on one page keep their class off.
    QString mode = QStringLiteral("never");
    switch (m_printOptions.codeSplit) {
    case PrintOptions::CodeSplit::SplitSmall: mode = QStringLiteral("small"); break;
    case PrintOptions::CodeSplit::SplitLarge: mode = QStringLiteral("large"); break;
    case PrintOptions::CodeSplit::NeverSplit: break;
    }
    QSizeF pagePt = parsePageSize(css);
    QMarginsF marginsPt = parsePageMargins(css);
    const double contentH = pagePt.height() - marginsPt.top() - marginsPt.bottom();

    const QString passJs = QStringLiteral(
        "(function(){"
        "  var mode='%1';"
        "  var contentH=%2;"
        "  if(mode==='never')return true;"
        "  var cls=(mode==='large')?'scriba-split-large':'scriba-split-small';"
        "  var els=document.querySelectorAll('pre');"
        "  for(var i=0;i<els.length;i++){"
        "    var r=els[i].getBoundingClientRect();"
        "    var hPt=r.height*72.0/96.0;"
        "    if(hPt>contentH)els[i].classList.add(cls);"
        "  }"
        "  return true;"
        "})()"
    ).arg(mode, QString::number(contentH));

    // Wait for async ECharts rendering (echarts.init + setOption stores a
    // Promise on window.echartsReady).  If there are no ec charts the promise
    // resolves immediately so this is a no-op for documents without charts.
    // Note: runJavaScript does not await promises on this Qt build (the
    // callback fires immediately), so the stock-chart render is additionally
    // gated by polling the `_scribaStockDone` flag the page sets once
    // initStockCharts() has run (see pollForStockFlag).
    m_hiddenEngine->page()->runJavaScript(
        QStringLiteral("Promise.all([window.echartsReady||Promise.resolve(),window.mermaidReady||Promise.resolve()]).then(function(){return true;})"),
        [this, genId, css, passJs](const QVariant &) {
            if (genId != m_generationId) return;
            pollForStockFlag(QStringLiteral("window._scribaStockDone===true"),
                [this, genId, css, passJs]() {
                    if (genId != m_generationId) return;
                    m_hiddenEngine->page()->runJavaScript(passJs, [this, genId, css](const QVariant &) {
                        if (genId != m_generationId) return;

                if (m_chromiumBinary.isEmpty()) {
                    qCDebug(lcPdf, "no chromium binary found, using Qt printToPdf (headers cannot be suppressed)");
                    QSizeF sizePt = parsePageSize(css);
                    QMarginsF m = parsePageMargins(css);
                    bool landscape = sizePt.width() > sizePt.height();
                    QSizeF normalPt = landscape
                        ? QSizeF(sizePt.height(), sizePt.width()) : sizePt;
                    QPageLayout layout(QPageSize(normalPt, QPageSize::Point),
                                       landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                                       m, QPageLayout::Point);
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
            });
        });
    });
}

void ExportPdfDialog::pollForStockFlag(const QString &probe,
                                       std::function<void()> continuation)
{
    m_stockPollProbe = probe;
    m_stockPollContinuation = std::move(continuation);
    m_stockPollAttempt = 0;
    if (!m_stockPollTimer) {
        m_stockPollTimer = new QTimer(this);
        m_stockPollTimer->setInterval(100);
        connect(m_stockPollTimer, &QTimer::timeout, this,
                &ExportPdfDialog::pollStockStep);
    }
    m_stockPollTimer->start();
    pollStockStep();
}

void ExportPdfDialog::pollStockStep()
{
    m_hiddenEngine->page()->runJavaScript(m_stockPollProbe,
        [this](const QVariant &ok) {
            if (ok.toBool() || ++m_stockPollAttempt >= 50) {
                m_stockPollTimer->stop();
                auto cont = std::move(m_stockPollContinuation);
                m_stockPollContinuation = nullptr;
                if (cont)
                    cont();
            }
        });
}

void ExportPdfDialog::generatePdfViaChromium(const QString &printCss)
{
    int genId = m_generationId;
    // Canvas stock charts are rasterized to PNG data-URLs before extraction:
    // promises can't be awaited through runJavaScript, so kick the bake and
    // poll a flag the conversion sets when done.
    m_hiddenEngine->page()->runJavaScript(
        QStringLiteral("if(typeof convertStockChartsToImages==='function'){"
                       "convertStockChartsToImages().then(function(){window._scribaStockBaked=true;});"
                       "}else{window._scribaStockBaked=true;}"),
        [this, genId, printCss](const QVariant &) {
            if (genId != m_generationId) return;
            pollForStockFlag(QStringLiteral("window._scribaStockBaked===true"),
                [this, genId, printCss]() { extractPdfBodyForChromium(genId, printCss); });
        });
}

void ExportPdfDialog::extractPdfBodyForChromium(int genId, const QString &printCss)
{
    if (genId != m_generationId) return;

    // Fix ECharts SVGs: bake explicit pixel dimensions from viewBox before
    // extraction.  The viewBox always carries intrinsic dimensions and
    // doesn't depend on widget layout (which may be stale for hidden views).
    // Mermaid SVGs already have explicit attributes and are unaffected.
    QString js = QStringLiteral(
        "(function() {"
        "  document.querySelectorAll('.echarts-chart svg').forEach(function(svg) {"
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

        bodyHtml = JsRenderEngine::replaceQrcUrls(bodyHtml);

        QString cspTag;
        if (QSettings().value(Preferences::BlockInlineHandlersExport, true).toBool()) {
            cspTag = QStringLiteral("<meta http-equiv=\"Content-Security-Policy\" content=\"%1\">").arg(Security::cspHeader(
                QSettings().value(Preferences::AllowExternalImagesExport, false).toBool()));
        }

        QString metaHead = QStringLiteral(
            "<meta charset=\"utf-8\">"
            "<base href=\"%1\">"
        ).arg(m_baseUrl.toHtmlEscaped());
        if (!cspTag.isEmpty())
            metaHead += cspTag;

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
        // A previous generation's chromium may still be running: QProcess::start()
        // is a silent no-op while the process is running, so without this the new
        // generation would never produce its PDF (and the stale run's finish would
        // only trip the old genId-guarded callback). Kill and reap it first, then
        // bind a fresh finished-handler for this generation.
        if (m_pdfProcess->state() != QProcess::NotRunning) {
            m_pdfProcess->kill();
            m_pdfProcess->waitForFinished();
        }
        QObject::disconnect(m_pdfProcess, &QProcess::finished, nullptr, nullptr);
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

void ExportPdfDialog::printDocument()
{
    if (m_pdfData.isEmpty())
        return;

    QPrinter printer;
    QSizeF pagePt = parsePageSize(m_currentPrintCss);
    if (pagePt.isValid() && pagePt.width() > 0 && pagePt.height() > 0) {
        QPageSize ps(pagePt, QPageSize::Point);
        if (ps.isValid())
            printer.setPageSize(ps);
    }

    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QEventLoop loop;
    connect(m_hiddenEngine, &QWebEngineView::printFinished,
            &loop, [&loop](bool success) {
                if (!success)
                    qCDebug(lcPdf, "print job failed");
                loop.quit();
            });
    m_hiddenEngine->print(&printer);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
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
