#include "MchemHelperDialog.h"
#include "StaticHelpers.h"
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

struct ChemEntry {
    const char *label;
    const char *formula;
};

struct ChemSection {
    const char *title;
    const ChemEntry *entries;
    int count;
};

static const ChemEntry kFormulasEntries[] = {
    {"Water", "H2O"},
    {"Carbon dioxide", "CO2"},
    {"Sulfuric acid", "H2SO4"},
    {"Acetic acid", "CH3COOH"},
    {"Glucose", "C6H12O6"},
    {"Sodium chloride", "NaCl"},
    {"Calcium carbonate", "CaCO3"},
    {"Iron(III) oxide", "Fe2O3"},
    {"Ammonia", "NH3"},
    {"Hydrochloric acid", "HCl"},
    {"Sodium hydroxide", "NaOH"},
    {"Nitric acid", "HNO3"},
    {"Ethanol", "C2H5OH"},
    {"Benzene", "C6H6"},
    {"Methane", "CH4"},
};
static const int kFormulasCount = sizeof(kFormulasEntries) / sizeof(ChemEntry);

static const ChemEntry kReactionsEntries[] = {
    {"Simple", "A + B -> C"},
    {"Equilibrium", "A + B <=> C"},
    {"With catalyst", "A ->[\\text{cat.}] B"},
    {"Redox", "Fe^{2+} -> Fe^{3+} + e^-"},
    {"Combustion", "CH4 + 2O2 -> CO2 + 2H2O"},
    {"Neutralization", "HCl + NaOH -> NaCl + H2O"},
    {"Decomposition", "2H2O -> 2H2 + O2"},
    {"Photosynthesis", "6CO2 + 6H2O ->[\\text{light}] C6H12O6 + 6O2"},
};
static const int kReactionsCount = sizeof(kReactionsEntries) / sizeof(ChemEntry);

static const ChemEntry kStatesEntries[] = {
    {"Gas", "(g)"},
    {"Liquid", "(l)"},
    {"Solid", "(s)"},
    {"Aqueous", "(aq)"},
};
static const int kStatesCount = sizeof(kStatesEntries) / sizeof(ChemEntry);

static const ChemEntry kArrowsEntries[] = {
    {"Yields", "->"},
    {"Resonance", "<->"},
    {"Equilibrium", "<=>"},
    {"Over", "->[\\text{above}]"},
    {"Under", "->[\\text{}][\\text{below}]"},
    {"Both", "->[\\text{above}][\\text{below}]"},
};
static const int kArrowsCount = sizeof(kArrowsEntries) / sizeof(ChemEntry);

static const ChemEntry kIsotopeEntries[] = {
    {"Carbon-14", "^{14}_{6}\\ce{C}"},
    {"Uranium-235", "^{235}_{92}\\ce{U}"},
    {"Proton", "^{1}_{1}\\ce{p}"},
    {"Neutron", "^{1}_{0}\\ce{n}"},
};
static const int kIsotopeCount = sizeof(kIsotopeEntries) / sizeof(ChemEntry);

static const ChemEntry kIonEntries[] = {
    {"Sodium ion", "Na^+"},
    {"Chloride ion", "Cl^-"},
    {"Sulfate", "SO4^{2-}"},
    {"Iron(III)", "Fe^{3+}"},
    {"Calcium", "Ca^{2+}"},
    {"Hydroxide", "OH^-"},
    {"Nitrate", "NO3^-"},
    {"Phosphate", "PO4^{3-}"},
};
static const int kIonCount = sizeof(kIonEntries) / sizeof(ChemEntry);

static const ChemSection kCheatSections[] = {
    {"Common Formulas", kFormulasEntries, kFormulasCount},
    {"Reactions", kReactionsEntries, kReactionsCount},
    {"State Symbols", kStatesEntries, kStatesCount},
    {"Reaction Arrows", kArrowsEntries, kArrowsCount},
    {"Isotopes", kIsotopeEntries, kIsotopeCount},
    {"Ions & Charges", kIonEntries, kIonCount},
};
static const int kCheatSectionCount = sizeof(kCheatSections) / sizeof(ChemSection);

MchemHelperDialog::MchemHelperDialog(const QString &themeCss, QWidget *parent)
    : QDialog(parent)
{
    CssUtils::ThemeColors colors = CssUtils::themeColors(themeCss);
    m_themeBg = colors.background;
    m_themeTxt = colors.text;

    setWindowTitle("Insert Chemistry Notation");
    resize(640, 700);

    m_previewTimer = new DebounceTimer(300, this);
    connect(m_previewTimer, &QTimer::timeout, this, &MchemHelperDialog::updatePreview);

    setupUi();
    updatePreview();
    m_input->setFocus();
}

void MchemHelperDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    m_preview = createPreviewView(this);
    m_preview->setFixedHeight(150);
    mainLayout->addWidget(m_preview);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    m_blockRadio = new QRadioButton("Block ($$…$$)", this);
    m_inlineRadio = new QRadioButton("Inline ($…$)", this);
    m_inlineRadio->setChecked(true);
    modeLayout->addWidget(m_blockRadio);
    modeLayout->addWidget(m_inlineRadio);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText("Type chemical formula, e.g. H2O, CO2, Fe^{3+}...");
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

    helperLayout->addWidget(createCheatSheet(helperWidget));
    helperLayout->addStretch();

    scrollArea->setWidget(helperWidget);
    scrollArea->viewport()->setStyleSheet(
        QString("background-color: %1;").arg(m_themeBg.name()));
    helperWidget->setStyleSheet(
        QString("background-color: %1;").arg(m_themeBg.name()));
    mainLayout->addWidget(scrollArea, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText("Ca&ncel");
    QPushButton *copyBtn = buttonBox->addButton("&Copy", QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton("&Insert", QDialogButtonBox::AcceptRole);
    Q_UNUSED(insertBtn);
    stripButtonIcons(buttonBox);
    mainLayout->addWidget(buttonBox);

    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedNotation());
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_input, &QPlainTextEdit::textChanged, this, &MchemHelperDialog::schedulePreviewUpdate);
    connect(m_blockRadio, &QRadioButton::toggled, this, &MchemHelperDialog::schedulePreviewUpdate);
}

QWidget *MchemHelperDialog::createCheatSheet(QWidget *parent)
{
    QGroupBox *group = new QGroupBox("Quick Reference", parent);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(4, 14, 4, 4);
    groupLayout->setSpacing(4);

    for (int s = 0; s < kCheatSectionCount; ++s) {
        const ChemSection &sec = kCheatSections[s];
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
            QLabel *nameLabel = new QLabel(
                QString("<span style='font-size:%1pt;'>%2</span>")
                    .arg(CssUtils::kUiFontSizePt)
                    .arg(QString::fromUtf8(sec.entries[i].label)), secBox);

            QTextBrowser *formulaLabel = new QTextBrowser(secBox);
            formulaLabel->setOpenExternalLinks(false);
            formulaLabel->setFrameShape(QFrame::NoFrame);
            formulaLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            formulaLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            formulaLabel->setMinimumWidth(120);
            formulaLabel->setFixedHeight(32);
            formulaLabel->setHtml(QString(
                "<div style='font-family:monospace;font-size:%4pt;padding:4px 8px;"
                "background-color:%1;color:%2;'>\\ce{%3}</div>")
                .arg(m_themeBg.name(), m_themeTxt.name(),
                     QString::fromUtf8(sec.entries[i].formula).replace("<", "&lt;").replace(">", "&gt;"))
                .arg(CssUtils::kUiFontSizePt));

            QPushButton *insertBtn = new QPushButton("+", secBox);
            insertBtn->setFixedSize(24, 24);
            insertBtn->setToolTip("Insert formula");
            connect(insertBtn, &QPushButton::clicked, this,
                [this, formula = QString::fromUtf8(sec.entries[i].formula)]() {
                    insertAtCursor(formula);
                });

            grid->addWidget(nameLabel, i, 0);
            grid->addWidget(formulaLabel, i, 1);
            grid->addWidget(insertBtn, i, 2);
            grid->setColumnStretch(1, 1);
        }

        groupLayout->addWidget(secBox);
    }

    return group;
}

void MchemHelperDialog::insertAtCursor(const QString &text)
{
    m_input->insertPlainText(text);
    m_input->setFocus();
}

void MchemHelperDialog::schedulePreviewUpdate()
{
    m_previewTimer->start();
}

QString MchemHelperDialog::generatedNotation() const
{
    QString chem = m_input->toPlainText().trimmed();
    if (chem.isEmpty())
        return QString();

    if (m_blockRadio->isChecked())
        return "$$\\ce{" + chem + "}$$";
    else
        return "$\\ce{" + chem + "}$";
}

void MchemHelperDialog::updatePreview()
{
    QString chem = m_input->toPlainText().trimmed();
    if (chem.isEmpty()) {
        m_preview->setHtml(QString(
            "<!DOCTYPE html><html><head><style>"
            "body{margin:0;display:flex;justify-content:center;align-items:center;"
            "min-height:100%;font-family:sans-serif;"
            "background-color:%1;color:%2;}</style></head>"
            "<body><span>Enter a chemical formula to see preview</span></body></html>")
            .arg(m_themeBg.name(), m_themeTxt.name()));
        return;
    }

    QString escaped = chem;
    escaped.replace("\\", "\\\\").replace("'", "\\'").replace("\n", " ");

    QString displayMode = m_blockRadio->isChecked() ? "true" : "false";

    QString html = QString(
        "<!DOCTYPE html><html><head>"
        "<meta charset=\"utf-8\">"
        "<link rel=\"stylesheet\" href=\"qrc:///katex.min.css\">"
        "<script src=\"qrc:///katex.min.js\"></script>"
        "<script src=\"qrc:///contrib/mhchem.min.js\"></script>"
        "<style>"
        "body{margin:0;display:flex;justify-content:center;align-items:center;"
        "min-height:100%;font-family:sans-serif;"
        "background-color:%3;color:%4;}"
        ".error{color:#d32f2f;padding:16px;font-size:14px;}"
        "</style>"
        "</head><body>"
        "<span id=\"chem\"></span>"
        "<script>"
        "try{"
        "katex.render('\\\\ce{%1}',document.getElementById('chem'),{"
        "throwOnError:true,displayMode:%2});"
        "}catch(e){"
        "document.body.innerHTML='<div class=\"error\">'+e+'</div>';"
        "}"
        "</script></body></html>"
    ).arg(escaped, displayMode, m_themeBg.name(), m_themeTxt.name());

    m_preview->setHtml(html);
}
