#pragma once

#include <QString>
#include <QTextCursor>
#include <QIcon>
#include <QChar>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QTimer>

inline const QChar clearSentinel(0x2412);

QString escapeJsString(const QString &s);
QString handleListReturn(const QString &line);
QString handleTableReturn(const QString &line, const QString &prevLine);
QString makeEmptyTableRow(int cols);
QString makeEmptyHtmlTableRow(int cols);
int tableNavCell(const QString &line, int cursorPos, bool forward);
int tableNavHtmlCell(const QString &line, int cursorPos, bool forward);
QString indentListLine(const QString &line);
QString outdentListLine(const QString &line);
QTextCursor restoreCursorPosition(QTextDocument *doc, int block, int column);

int countSentences(const QString &text);
int estimateSyllables(const QString &word);
double fleschKincaidGrade(int words, int sentences, int syllables);

QIcon themedIcon(const QString &svgPath, const QColor &color, int size = 28);

void stripButtonIcon(QAbstractButton *btn);
void stripButtonIcons(QDialogButtonBox *box);

QString readResourceFile(const QString &path);

class DebounceTimer : public QTimer
{
    Q_OBJECT
public:
    explicit DebounceTimer(int interval, QObject *parent = nullptr);
    void arm();
};

class QWebEngineView;
