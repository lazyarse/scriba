#include "CssUtils.h"
#include <QRegularExpression>
#include <QColor>

namespace CssUtils {

QString deriveChromeCss(const QString &themeCss)
{
    auto extractBg = [&](const QString &selector) {
        QRegularExpression re(
            R"(\b)" + selector + R"(\s*\{[^}]*background(?:-color)?\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    auto extractColor = [&](const QString &selector) {
        QRegularExpression re(
            R"(\b)" + selector + R"(\s*\{(?:[^}]*;\s*)?\bcolor\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    QString bgStr = extractBg("#editor");
    if (bgStr.isEmpty())
        bgStr = extractBg("body");

    QString txtStr = extractColor("#editor");
    if (txtStr.isEmpty())
        txtStr = extractColor("body");

    QColor bg(QStringLiteral("#ffffff"));
    if (!bgStr.isEmpty()) {
        QColor parsed(bgStr);
        if (parsed.isValid())
            bg = parsed;
    }

    bool dark = bg.lightness() < 128;
    QColor track, thumb, hover, selBg, txt, selTxt;
    if (dark) {
        track = bg.lighter(160);
        thumb = bg.lighter(220);
        hover = bg.lighter(250);
        selBg = hover;
        txt = QColor(QStringLiteral("#f0f0f0"));
        selTxt = QColor(QStringLiteral("#ffffff"));
    } else {
        track = bg.darker(115);
        thumb = bg.darker(160);
        hover = bg.darker(180);
        selBg = hover;
        txt = QColor(QStringLiteral("#333333"));
        selTxt = QColor(QStringLiteral("#000000"));
    }

    return QStringLiteral(
        "QDialog { background-color: %2; }\n"
        "QGroupBox { color: %3; font-weight: bold; border: 1px solid %4; margin-top: 8px; }\n"
        "QGroupBox::title { color: %3; font-weight: bold; }\n"
        "QCheckBox { color: %3; spacing: 6px; }\n"
        "QCheckBox::indicator { width: 14px; height: 14px; background-color: %2; border: 1px solid %4; }\n"
        "QCheckBox::indicator:checked { background-color: %5; border: 1px solid %5; image: url(:/checkbox-checked.svg); }\n"
        "QRadioButton { color: %3; spacing: 6px; }\n"
        "QRadioButton::indicator { width: 14px; height: 14px; background-color: %2; border: 1px solid %4; border-radius: 7px; }\n"
        "QRadioButton::indicator:checked { background-color: %5; border: 1px solid %5; }\n"
        "QListWidget { background-color: %2; color: %3; border: none; }\n"
        "QListWidget::item:selected { background-color: %5; color: %6; }\n"
        "QListWidget::item:hover { background-color: %1; }\n"
        "QPushButton { background-color: %4; color: %3; border: 1px solid %4; padding: 4px 12px; }\n"
        "QPushButton:hover { background-color: %5; }\n"
        "QLabel { color: %3; }\n"
        "QMenuBar { background-color: %2; color: %3; }\n"
        "QMenuBar::item:selected { background-color: %5; }\n"
        "#scriba-editor { padding: 0 !important; margin: 0 !important; border: none !important; background-color: %7 !important; color: %8 !important; }\n"
        "QSplitter::handle { background-color: %4; width: 1px; }\n"
        "QSplitter::handle:hover { background-color: %5; }\n"
        "QScrollBar:vertical { background: %2; width: 12px; }\n"
        "QScrollBar::handle:vertical { background: %4; border-radius: 6px; min-height: 30px; }\n"
        "QScrollBar::handle:vertical:hover { background: %5; }\n"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }\n"
        "QScrollBar:horizontal { background: %2; height: 12px; }\n"
        "QScrollBar::handle:horizontal { background: %4; border-radius: 6px; min-width: 30px; }\n"
        "QScrollBar::handle:horizontal:hover { background: %5; }\n"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }\n"
        
        "::-webkit-scrollbar { width: 12px; height: 12px; }\n"
        "::-webkit-scrollbar-track { background: %2; }\n"
        "::-webkit-scrollbar-thumb { background: %4; border-radius: 6px; }\n"
        "::-webkit-scrollbar-thumb:hover { background: %5; }\n"
    ).arg(
        track.name(),   // %1 — splitter handle, hover bg
        track.name(),   // %2 — dialog/bg, menus, scrollbar track
        txt.name(),     // %3 — text color
        thumb.name(),   // %4 — button bg, scrollbar handle
        hover.name(),   // %5 — hover/selected bg
        selTxt.name(),  // %6 — selected text color
        bg.name(),       // %7 — editor background
        txtStr.isEmpty() ? txt.name() : txtStr  // %8 — editor text color from theme
    );
}

} // namespace CssUtils
