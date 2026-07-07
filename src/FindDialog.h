#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindDialog(QWidget *parent = nullptr);

    QString searchTerm() const;
    bool regexEnabled() const;
    bool caseSensitive() const;

private:
    QLineEdit *m_searchInput;
    QCheckBox *m_regexCheck;
    QCheckBox *m_caseCheck;
};

#endif
