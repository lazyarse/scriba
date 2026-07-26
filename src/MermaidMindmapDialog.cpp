#include "MermaidMindmapDialog.h"
#include "Preview.h"

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QClipboard>
#include <QCheckBox>
#include <QSpinBox>
#include <QWebEngineView>

MermaidMindmapDialog::MermaidMindmapDialog(const QString &themeCss, QWidget *parent)
    : MermaidDialogBase(tr("Insert Mermaid Mindmap"), themeCss, parent)
{
    resize(900, 550);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left panel
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabel(tr("Text"));
    m_tree->setColumnCount(1);
    leftLayout->addWidget(m_tree);

    auto *treeButtons = new QHBoxLayout;
    auto *addChildBtn = new QPushButton(tr("Add Child"));
    auto *addSiblingBtn = new QPushButton(tr("Add Sibling"));
    auto *deleteBtn = new QPushButton(tr("Delete"));
    treeButtons->addWidget(addChildBtn);
    treeButtons->addWidget(addSiblingBtn);
    treeButtons->addWidget(deleteBtn);
    leftLayout->addLayout(treeButtons);

    connect(addChildBtn, &QPushButton::clicked, this, &MermaidMindmapDialog::addChild);
    connect(addSiblingBtn, &QPushButton::clicked, this, &MermaidMindmapDialog::addSibling);
    connect(deleteBtn, &QPushButton::clicked, this, &MermaidMindmapDialog::deleteNode);

    // Right panel
    auto *rightWidget = new QWidget;
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
    rightLayout->addWidget(m_preview);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Ca&ncel"));
    QPushButton *copyBtn = buttonBox->addButton(tr("&Copy"), QDialogButtonBox::ActionRole);
    QPushButton *insertBtn = buttonBox->addButton(tr("&Insert"), QDialogButtonBox::AcceptRole);
    for (auto *btn : buttonBox->buttons())
        btn->setIcon(QIcon());
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(generatedDiagram());
    });
    mainLayout->addWidget(buttonBox);

    // Default data
    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, tr("Central Idea"));
    root->setExpanded(true);

    auto *idea1 = new QTreeWidgetItem(root);
    idea1->setText(0, tr("Idea 1"));

    auto *subIdea = new QTreeWidgetItem(idea1);
    subIdea->setText(0, tr("Sub-idea"));

    auto *idea2 = new QTreeWidgetItem(root);
    idea2->setText(0, tr("Idea 2"));

    m_tree->setCurrentItem(root);

    connect(m_tree, &QTreeWidget::itemChanged, this, [this]() { schedulePreviewUpdate(); });

    updatePreview();
    schedulePreviewUpdate();
}

QString MermaidMindmapDialog::buildDiagram() const
{
    QString out = "mindmap\n";
    QTreeWidgetItem *root = m_tree->topLevelItem(0);
    if (root)
        out += buildNode(root, 1);
    return out;
}

QString MermaidMindmapDialog::buildNode(QTreeWidgetItem *item, int depth) const
{
    QString indent(depth * 4, ' ');
    QString text = item->text(0);
    QString out;
    if (depth == 1)
        out = indent + "root((" + text + "))\n";
    else
        out = indent + text + "\n";
    for (int i = 0; i < item->childCount(); ++i)
        out += buildNode(item->child(i), depth + 1);
    return out;
}

void MermaidMindmapDialog::addChild()
{
    auto *parent = m_tree->currentItem();
    if (!parent) return;

    auto *child = new QTreeWidgetItem(parent);
    child->setText(0, tr("New node"));
    parent->setExpanded(true);
    m_tree->setCurrentItem(child);
    schedulePreviewUpdate();
}

void MermaidMindmapDialog::addSibling()
{
    auto *current = m_tree->currentItem();
    if (!current) return;

    auto *parent = current->parent();
    auto *sibling = new QTreeWidgetItem(parent ? parent : m_tree->invisibleRootItem());
    sibling->setText(0, tr("New node"));
    m_tree->setCurrentItem(sibling);
    schedulePreviewUpdate();
}

void MermaidMindmapDialog::deleteNode()
{
    auto *current = m_tree->currentItem();
    if (!current) return;
    if (current == m_tree->topLevelItem(0)) return;

    auto *parent = current->parent();
    if (parent) {
        parent->removeChild(current);
    } else {
        m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(current));
    }
    schedulePreviewUpdate();
}
