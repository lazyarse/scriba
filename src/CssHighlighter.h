#ifndef CSSHIGHLIGHTER_H
#define CSSHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class CssHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CssHighlighter(const QString &themeCss, QTextDocument *parent);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_punctFormat;
};

#endif
