#pragma once

#include "MermaidDialogBase.h"

class QComboBox;
class QTableWidget;
class QLineEdit;
class QCheckBox;

class MermaidGanttDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidGanttDialog(const QString &themeCss, QWidget *parent = nullptr);

private:
    void setupUi();
    QString buildDiagram() const override;

    QLineEdit *m_titleEdit;
    QComboBox *m_dateFormatCombo;
    QCheckBox *m_excludeWeekendsCheck;
    QTableWidget *m_taskTable;
};

