#include "Editor.h"
#include "Preferences.h"
#include <QKeyEvent>
#include <QSettings>
#include <QFont>

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

    QFont font(family, size);
    setFont(font);

    QPalette pal = palette();
    pal.setColor(QPalette::Text, color);
    setPalette(pal);
}

void Editor::applyFontSettings(const QString &family, int size, const QColor &color)
{
    QSettings settings;
    settings.setValue(Preferences::EditorFont, family);
    settings.setValue(Preferences::EditorFontSize, size);
    settings.setValue(Preferences::EditorFontColor, color.name());

    QFont font(family, size);
    setFont(font);

    QPalette pal = palette();
    pal.setColor(QPalette::Text, color);
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
