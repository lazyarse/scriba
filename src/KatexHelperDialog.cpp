#include "KatexHelperDialog.h"
#include "Preview.h"
#include "CssUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QWebEngineView>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QTimer>
#include <QIcon>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>

struct SymbolEntry {
    const char *label;
    const char *latex;
};

static const SymbolEntry kGreekLower[] = {
    {"α", "\\alpha"}, {"β", "\\beta"}, {"γ", "\\gamma"}, {"δ", "\\delta"},
    {"ε", "\\epsilon"}, {"ζ", "\\zeta"}, {"η", "\\eta"}, {"θ", "\\theta"},
    {"ι", "\\iota"}, {"κ", "\\kappa"}, {"λ", "\\lambda"}, {"μ", "\\mu"},
    {"ν", "\\nu"}, {"ξ", "\\xi"}, {"π", "\\pi"}, {"ρ", "\\rho"},
    {"σ", "\\sigma"}, {"τ", "\\tau"}, {"υ", "\\upsilon"}, {"φ", "\\phi"},
    {"χ", "\\chi"}, {"ψ", "\\psi"}, {"ω", "\\omega"},
};
static const int kGreekLowerCount = sizeof(kGreekLower) / sizeof(SymbolEntry);

static const SymbolEntry kGreekUpper[] = {
    {"Α", "\\Alpha"}, {"Β", "\\Beta"}, {"Γ", "\\Gamma"}, {"Δ", "\\Delta"},
    {"Ε", "\\Epsilon"}, {"Ζ", "\\Zeta"}, {"Η", "\\Eta"}, {"Θ", "\\Theta"},
    {"Ι", "\\Iota"}, {"Κ", "\\Kappa"}, {"Λ", "\\Lambda"}, {"Μ", "\\Mu"},
    {"Ν", "\\Nu"}, {"Ξ", "\\Xi"}, {"Π", "\\Pi"}, {"Ρ", "\\Rho"},
    {"Σ", "\\Sigma"}, {"Τ", "\\Tau"}, {"Υ", "\\Upsilon"}, {"Φ", "\\Phi"},
    {"Χ", "\\Chi"}, {"Ψ", "\\Psi"}, {"Ω", "\\Omega"},
};
static const int kGreekUpperCount = sizeof(kGreekUpper) / sizeof(SymbolEntry);

static const SymbolEntry kOperators[] = {
    {"±", "\\pm"}, {"∓", "\\mp"}, {"×", "\\times"}, {"÷", "\\div"},
    {"·", "\\cdot"}, {"∘", "\\circ"}, {"⊕", "\\oplus"}, {"⊗", "\\otimes"},
    {"∩", "\\cap"}, {"∪", "\\cup"}, {"∨", "\\vee"}, {"∧", "\\wedge"},
    {"¬", "\\neg"}, {"∖", "\\setminus"}, {"†", "\\dagger"}, {"‡", "\\ddagger"},
};
static const int kOperatorsCount = sizeof(kOperators) / sizeof(SymbolEntry);

static const SymbolEntry kRelations[] = {
    {"=", "="}, {"≠", "\\neq"}, {"<", "<"}, {">", ">"},
    {"≤", "\\leq"}, {"≥", "\\geq"}, {"≈", "\\approx"}, {"∼", "\\sim"},
    {"≡", "\\equiv"}, {"∝", "\\propto"}, {"∈", "\\in"}, {"∉", "\\notin"},
    {"⊂", "\\subset"}, {"⊃", "\\supset"}, {"⊆", "\\subseteq"}, {"⊇", "\\supseteq"},
};
static const int kRelationsCount = sizeof(kRelations) / sizeof(SymbolEntry);

static const SymbolEntry kArrows[] = {
    {"←", "\\leftarrow"}, {"→", "\\rightarrow"}, {"↔", "\\leftrightarrow"},
    {"⇐", "\\Leftarrow"}, {"⇒", "\\Rightarrow"}, {"⇔", "\\Leftrightarrow"},
    {"↑", "\\uparrow"}, {"↓", "\\downarrow"}, {"↕", "\\updownarrow"},
    {"⇑", "\\Uparrow"}, {"⇓", "\\Downarrow"}, {"⇕", "\\Updownarrow"},
    {"↦", "\\mapsto"}, {"↗", "\\nearrow"}, {"↘", "\\searrow"},
};
static const int kArrowsCount = sizeof(kArrows) / sizeof(SymbolEntry);

static const SymbolEntry kStructures[] = {
    {"∑", "\\sum"}, {"∏", "\\prod"}, {"∫", "\\int"}, {"∮", "\\oint"},
    {"√", "\\sqrt{}"}, {"∞", "\\infty"}, {"∂", "\\partial"}, {"∇", "\\nabla"},
    {"ℵ", "\\aleph"}, {"∀", "\\forall"}, {"∃", "\\exists"}, {"¬", "\\neg"},
    {"⟨", "\\langle"}, {"⟩", "\\rangle"}, {"⌊", "\\lfloor"}, {"⌋", "\\rfloor"},
};
static const int kStructuresCount = sizeof(kStructures) / sizeof(SymbolEntry);

struct CheatEntry {
    const char *command;
    const char *example;
};

struct CheatSection {
    const char *title;
    const CheatEntry *entries;
    int count;
};

static const CheatEntry kFractionsEntries[] = {
    {"\\frac{a}{b}", "\\frac{a}{b}"},
    {"\\dfrac{a}{b}", "\\dfrac{a}{b}"},
    {"\\cfrac{a}{b}", "\\cfrac{a}{b}"},
    {"\\sqrt{x}", "\\sqrt{x}"},
    {"\\sqrt[n]{x}", "\\sqrt[n]{x}"},
};
static const CheatEntry kSumsEntries[] = {
    {"\\sum_{i=0}^{n}", "\\sum_{i=0}^{n}"},
    {"\\prod_{k=1}^{n}", "\\prod_{k=1}^{n}"},
    {"\\int_{a}^{b}", "\\int_{a}^{b}"},
    {"\\iint", "\\iint"},
    {"\\oint_{C}", "\\oint_{C}"},
    {"\\lim_{x \\to 0}", "\\lim_{x \\to 0}"},
};
static const CheatEntry kMatrixEntries[] = {
    {"\\begin{pmatrix}a\\\\b\\end{pmatrix}", "\\begin{pmatrix}a\\\\b\\end{pmatrix}"},
    {"\\begin{bmatrix}a\\\\b\\end{bmatrix}", "\\begin{bmatrix}a\\\\b\\end{bmatrix}"},
    {"\\begin{vmatrix}a\\\\b\\end{vmatrix}", "\\begin{vmatrix}a\\\\b\\end{vmatrix}"},
    {"\\begin{Bmatrix}a\\\\b\\end{Bmatrix}", "\\begin{Bmatrix}a\\\\b\\end{Bmatrix}"},
};
static const CheatEntry kDecorEntries[] = {
    {"\\hat{x}", "\\hat{x}"},
    {"\\bar{x}", "\\bar{x}"},
    {"\\vec{x}", "\\vec{x}"},
    {"\\dot{x}", "\\dot{x}"},
    {"\\ddot{x}", "\\ddot{x}"},
    {"\\tilde{x}", "\\tilde{x}"},
    {"\\overline{AB}", "\\overline{AB}"},
    {"\\underline{AB}", "\\underline{AB}"},
    {"\\overbrace{a+b}^{n}", "\\overbrace{a+b}^{n}"},
    {"\\underbrace{a+b}_{n}", "\\underbrace{a+b}_{n}"},
};
static const CheatEntry kTextEntries[] = {
    {"\\text{hello}", "\\text{hello}"},
    {"\\mathbf{A}", "\\mathbf{A}"},
    {"\\mathit{x}", "\\mathit{x}"},
    {"\\mathbb{R}", "\\mathbb{R}"},
    {"\\mathcal{L}", "\\mathcal{L}"},
    {"\\mathrm{d}", "\\mathrm{d}"},
};

static const CheatSection kCheatSections[] = {
    {"Fractions & Roots", kFractionsEntries, 5},
    {"Sums & Integrals", kSumsEntries, 6},
    {"Matrices", kMatrixEntries, 4},
    {"Decorations", kDecorEntries, 10},
    {"Text & Styling", kTextEntries, 6},
};
static const int kCheatSectionCount = sizeof(kCheatSections) / sizeof(CheatSection);

KatexHelperDialog::KatexHelperDialog(const QString &themeCss, QWidget *parent)
    : QDialog(parent)
{
    CssUtils::ThemeColors colors = CssUtils::themeColors(themeCss);
    m_themeBg = colors.background;
    m_themeTxt = colors.text;

    setWindowTitle("Insert Equation");
    resize(640, 700);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &KatexHelperDialog::updatePreview);

    setupUi();
    updatePreview();
}

void KatexHelperDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    m_preview = new QWebEngineView(this);
    m_preview->setPage(new PreviewPage(m_preview));
    m_preview->setFixedHeight(150);
    mainLayout->addWidget(m_preview);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    m_blockRadio = new QRadioButton("Block ($$…$$)", this);
    m_inlineRadio = new QRadioButton("Inline ($…$)", this);
    m_blockRadio->setChecked(true);
    modeLayout->addWidget(m_blockRadio);
    modeLayout->addWidget(m_inlineRadio);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText("Type LaTeX here...");
    m_input->setFixedHeight(80);
    QFont mono("Monospace");
    mono.setStyleHint(QFont::Monospace);
    m_input->setFont(mono);
    m_input->setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; color: %2; border: none; }")
        .arg(m_themeBg.name(), m_themeTxt.name()));
    mainLayout->addWidget(m_input);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *helperWidget = new QWidget();
    QVBoxLayout *helperLayout = new QVBoxLayout(helperWidget);
    helperLayout->setContentsMargins(0, 0, 0, 0);
    helperLayout->setSpacing(8);

    helperLayout->addWidget(createSymbolPalette(helperWidget));
    helperLayout->addWidget(createCheatSheet(helperWidget));
    helperLayout->addStretch();

    scrollArea->setWidget(helperWidget);
    scrollArea->viewport()->setStyleSheet(
        QString("background-color: %1;").arg(m_themeBg.name()));
    helperWidget->setStyleSheet(
        QString("background-color: %1;").arg(m_themeBg.name()));
    mainLayout->addWidget(scrollArea, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *copyBtn = buttonBox->addButton("Copy", QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton("Insert", QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedLatex());
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_input, &QPlainTextEdit::textChanged, this, &KatexHelperDialog::schedulePreviewUpdate);
    connect(m_blockRadio, &QRadioButton::toggled, this, &KatexHelperDialog::schedulePreviewUpdate);
}

QWidget *KatexHelperDialog::createSymbolPalette(QWidget *parent)
{
    QGroupBox *group = new QGroupBox("Symbol Palette", parent);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(4, 8, 4, 4);
    groupLayout->setSpacing(4);

    auto addCategory = [&](const char *title, const SymbolEntry *entries, int count) {
        QFrame *frame = new QFrame(group);
        frame->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout *vbox = new QVBoxLayout(frame);
        vbox->setContentsMargins(4, 4, 4, 4);
        vbox->setSpacing(2);

        QLabel *label = new QLabel(QString("<b>%1</b>").arg(title), frame);
        label->setStyleSheet("font-size: 11px;");
        vbox->addWidget(label);

        QGridLayout *grid = new QGridLayout();
        grid->setSpacing(2);
        int col = 0;
        int row = 0;
        for (int i = 0; i < count; ++i) {
            QPushButton *btn = new QPushButton(QString::fromUtf8(entries[i].label), frame);
            btn->setFixedSize(36, 28);
            btn->setToolTip(QString::fromUtf8(entries[i].latex));
            QFont f = btn->font();
            f.setPointSize(11);
            btn->setFont(f);
            connect(btn, &QPushButton::clicked, this, [this, latex = QString::fromUtf8(entries[i].latex)]() {
                insertAtCursor(latex);
            });
            grid->addWidget(btn, row, col);
            if (++col >= 8) {
                col = 0;
                ++row;
            }
        }
        if (col > 0)
            grid->setColumnStretch(col, 1);
        vbox->addLayout(grid);

        groupLayout->addWidget(frame);
    };

    addCategory("Greek (lower)", kGreekLower, kGreekLowerCount);
    addCategory("Greek (upper)", kGreekUpper, kGreekUpperCount);
    addCategory("Operators", kOperators, kOperatorsCount);
    addCategory("Relations", kRelations, kRelationsCount);
    addCategory("Arrows", kArrows, kArrowsCount);
    addCategory("Structures", kStructures, kStructuresCount);

    return group;
}

QWidget *KatexHelperDialog::createCheatSheet(QWidget *parent)
{
    QGroupBox *group = new QGroupBox("Cheat Sheet", parent);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(4, 8, 4, 4);
    groupLayout->setSpacing(4);

    for (int s = 0; s < kCheatSectionCount; ++s) {
        const CheatSection &sec = kCheatSections[s];
        QGroupBox *secBox = new QGroupBox(QString::fromUtf8(sec.title), group);
        secBox->setCheckable(true);
        {
            bool dark = m_themeBg.lightness() < 128;
            QColor borderColor = dark ? m_themeBg.lighter(220) : m_themeBg.darker(125);
            secBox->setStyleSheet(QString(
                "QGroupBox { color: %1; font-weight: bold; "
                "border: 1px solid %2; margin-top: 8px; padding-top: 18px; }"
                "QGroupBox::title { color: %1; font-weight: bold; }")
                .arg(m_themeTxt.name(), borderColor.name()));
        }
        QGridLayout *grid = new QGridLayout(secBox);
        grid->setContentsMargins(4, 4, 4, 4);
        grid->setSpacing(2);

        for (int i = 0; i < sec.count; ++i) {
            QLabel *cmdLabel = new QLabel(
                QString("<code style='font-size:11px;'>%1</code>")
                    .arg(QString::fromUtf8(sec.entries[i].command)), secBox);
            QTextBrowser *exLabel = new QTextBrowser(secBox);
            exLabel->setOpenExternalLinks(false);
            exLabel->setFrameShape(QFrame::NoFrame);
            exLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            exLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            exLabel->setMinimumWidth(120);
            exLabel->setFixedHeight(32);
            exLabel->setHtml(QString(
                "<div style='font-family:monospace;font-size:12px;padding:4px 8px;"
                "background-color:%1;color:%2;'>%3</div>")
                .arg(m_themeBg.name(), m_themeTxt.name(),
                     QString::fromUtf8(sec.entries[i].example).replace("<", "&lt;").replace(">", "&gt;")));

            QPushButton *insertBtn = new QPushButton("+", secBox);
            insertBtn->setFixedSize(24, 24);
            insertBtn->setToolTip("Insert command");
            connect(insertBtn, &QPushButton::clicked, this, [this, latex = QString::fromUtf8(sec.entries[i].command)]() {
                insertAtCursor(latex);
            });

            grid->addWidget(cmdLabel, i, 0);
            grid->addWidget(exLabel, i, 1);
            grid->addWidget(insertBtn, i, 2);
            grid->setColumnStretch(1, 1);
        }

        groupLayout->addWidget(secBox);
    }

    return group;
}

void KatexHelperDialog::insertAtCursor(const QString &text)
{
    m_input->insertPlainText(text);
    m_input->setFocus();
}

void KatexHelperDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

QString KatexHelperDialog::generatedLatex() const
{
    QString latex = m_input->toPlainText().trimmed();
    if (latex.isEmpty())
        return QString();

    if (m_blockRadio->isChecked())
        return "$$" + latex + "$$";
    else
        return "$" + latex + "$";
}

void KatexHelperDialog::updatePreview()
{
    QString latex = m_input->toPlainText().trimmed();
    if (latex.isEmpty()) {
        m_preview->setHtml(QString(
            "<!DOCTYPE html><html><head><style>"
            "body{margin:0;display:flex;justify-content:center;align-items:center;"
            "min-height:100%;font-family:sans-serif;"
            "background-color:%1;color:%2;}</style></head>"
            "<body><span>Enter LaTeX to see preview</span></body></html>")
            .arg(m_themeBg.name(), m_themeTxt.name()));
        return;
    }

    QString escaped = latex;
    escaped.replace("\\", "\\\\").replace("'", "\\'").replace("\n", " ");

    QString displayMode = m_blockRadio->isChecked() ? "true" : "false";

    QString html = QString(
        "<!DOCTYPE html><html><head>"
        "<meta charset=\"utf-8\">"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<style>"
        "body{margin:0;display:flex;justify-content:center;align-items:center;"
        "min-height:100%;font-family:sans-serif;"
        "background-color:%3;color:%4;}"
        ".error{color:#d32f2f;padding:16px;font-size:14px;}"
        "</style>"
        "</head><body>"
        "<span id=\"math\"></span>"
        "<script>"
        "try{"
        "katex.render('%1',document.getElementById('math'),{"
        "throwOnError:true,displayMode:%2});"
        "}catch(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';"
        "}"
        "</script></body></html>"
    ).arg(escaped, displayMode, m_themeBg.name(), m_themeTxt.name());

    m_preview->setHtml(html);
}
