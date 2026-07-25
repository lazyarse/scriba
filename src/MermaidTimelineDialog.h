#ifndef MERMAIDTIMELINEDIALOG_H
#define MERMAIDTIMELINEDIALOG_H

#include "MermaidDialogBase.h"

class QTableWidget;
class QLineEdit;

class MermaidTimelineDialog : public MermaidDialogBase
{
    Q_OBJECT

public:
    explicit MermaidTimelineDialog(const QString &themeCss, QWidget *parent = nullptr);

protected:
    QString buildDiagram() const override;

private:
    void setupUi();

    QLineEdit *m_titleEdit;
    QTableWidget *m_table;
};

#endif
