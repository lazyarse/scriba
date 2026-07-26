#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTextDocument>

class FindDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindDialog(QWidget *parent = nullptr);

    QString searchTerm() const;
    QString replaceTerm() const;
    bool regexEnabled() const;
    bool caseSensitive() const;

    void focusSearchInput();
    void focusReplaceInput();

    void setMatchCount(int count);
    QString matchCountText() const;

signals:
    void findNextRequested(const QString &text, bool useRegex, bool caseSensitive);
    void findPrevRequested(const QString &text, bool useRegex, bool caseSensitive);
    void searchTextChanged(const QString &text, bool useRegex, bool caseSensitive);
    void replaceRequested(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);
    void replaceAllRequested(const QString &search, const QString &replacement, bool useRegex, bool caseSensitive);

private slots:
    void emitFindNext();
    void emitFindPrev();
    void emitReplace();
    void emitReplaceAll();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLineEdit *m_searchInput;
    QLineEdit *m_replaceInput;
    QCheckBox *m_regexCheck;
    QCheckBox *m_caseCheck;
    QLabel *m_matchCountLabel;
};

