#include "Editor.h"
#include "Preferences.h"
#include <QKeyEvent>
#include <QSettings>
#include <QFont>
#include <QTextBlock>

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setPlaceholderText("Start writing markdown...");
    setTabStopDistance(40);
    loadSettings();
}

void Editor::loadSettings()
{
    QSettings settings;
    QString family = settings.value(Preferences::EditorFont, "Monospace").toString();
    int size = settings.value(Preferences::EditorFontSize, 14).toInt();
    QColor color(settings.value(Preferences::EditorFontColor, "#333333").toString());
    QColor bgColor(settings.value(Preferences::EditorBgColor, "#ffffff").toString());

    QFont font(family, size);
    setFont(font);

    QPalette pal = palette();
    pal.setColor(QPalette::Text, color);
    pal.setColor(QPalette::Base, bgColor);
    setPalette(pal);
}

void Editor::applyFontSettings(const QString &family, int size, const QColor &color, const QColor &bgColor)
{
    QSettings settings;
    settings.setValue(Preferences::EditorFont, family);
    settings.setValue(Preferences::EditorFontSize, size);
    settings.setValue(Preferences::EditorFontColor, color.name());
    settings.setValue(Preferences::EditorBgColor, bgColor.name());

    QFont font(family, size);
    setFont(font);

    QPalette pal = palette();
    pal.setColor(QPalette::Text, color);
    pal.setColor(QPalette::Base, bgColor);
    setPalette(pal);
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

int Editor::firstVisibleLineNumber() const
{
    return firstVisibleBlock().blockNumber() + 1;
}
