#pragma once

#include "MermaidDialogBase.h"

class QComboBox;
class QTableWidget;

class MermaidStateDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidStateDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void onStateChanged();

private:
    void setupUi();
    void refreshTransitionCombos();
    QString buildDiagram() const override;

    QTableWidget *m_stateTable;
    QTableWidget *m_transitionTable;
};

