#pragma once

#include "MermaidDialogBase.h"

class QComboBox;
class QTableWidget;

class MermaidSequenceDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidSequenceDialog(const QString &themeCss, QWidget *parent = nullptr);

private slots:
    void onParticipantChanged();

private:
    void setupUi();
    void refreshMessageCombos();
    QString buildDiagram() const override;

    QTableWidget *m_participantTable;
    QTableWidget *m_messageTable;
};

