// Copyright (C) 2026 LazyArse
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#include "CssUtils.h"
#include <QRegularExpression>
#include <QStandardPaths>

namespace CssUtils {

QString scribaConfigDir()
{
    const QByteArray overrideDir = qgetenv("SCRIBA_TEST_CONFIG_DIR");
    if (!overrideDir.isEmpty())
        return QString::fromUtf8(overrideDir);
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/scriba";
}

static QColor mix(const QColor &a, const QColor &b, int percentOfB)
{
    return QColor(
        a.red() + (b.red() - a.red()) * percentOfB / 100,
        a.green() + (b.green() - a.green()) * percentOfB / 100,
        a.blue() + (b.blue() - a.blue()) * percentOfB / 100);
}

QColor chromeTextColor(const QString &themeCss)
{
    auto extractBg = [&](const QString &selector) {
        QRegularExpression re(
            selector + R"(\s*\{[^}]*background(?:-color)?\s*:\s*([^;\}]+))"
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

    QColor bg(QStringLiteral("#ffffff"));
    if (!bgStr.isEmpty()) {
        QColor parsed(bgStr);
        if (parsed.isValid())
            bg = parsed;
    }

    return bg.lightness() < 128
        ? QColor(QStringLiteral("#f0f0f0"))
        : QColor(QStringLiteral("#333333"));
}

QString deriveChromeCss(const QString &themeCss, int uiFontSizePt)
{
    auto extractBg = [&](const QString &selector) {
        QRegularExpression re(
            selector + R"(\s*\{[^}]*background(?:-color)?\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    auto extractColor = [&](const QString &selector) {
        QRegularExpression re(
            selector + R"(\s*\{(?:[^}]*;\s*)?\bcolor\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    auto extractVar = [&](const QString &name) -> QString {
        QRegularExpression re(QStringLiteral("--%1\\s*:\\s*([^;]+)").arg(name),
                               QRegularExpression::MultilineOption);
        QString result;
        auto it = re.globalMatch(themeCss);
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

    QString chkBgStr = extractVar("checkbox-checked-bg");

    QColor bg(QStringLiteral("#ffffff"));
    if (!bgStr.isEmpty()) {
        QColor parsed(bgStr);
        if (parsed.isValid())
            bg = parsed;
    }

    bool dark = bg.lightness() < 128;
    QColor track, thumb, hover, sideBg, selBg, txt, selTxt, dim;
    QColor btnBorder;
    QColor chkBg, chkCheckedBg;
    QString chkImg, upArrowImg, downArrowImg, radioImg;
    txt = chromeTextColor(themeCss);
    if (dark) {
        track = bg.lighter(160);
        thumb = bg.lighter(220);
        hover = bg.lighter(250);
        sideBg = bg.lighter(130);
        selBg = hover;
        selTxt = QColor(QStringLiteral("#ffffff"));
        dim = QColor(QStringLiteral("#999999"));
        chkBg = track;
        chkImg = QStringLiteral("url(:/checkbox-checked.svg)");
        upArrowImg = QStringLiteral("url(:/arrow-up.svg)");
        downArrowImg = QStringLiteral("url(:/arrow-down.svg)");
        radioImg = QStringLiteral("url(:/radio-dot.svg)");
        btnBorder = thumb.lighter(150);
    } else {
        track = bg.darker(105);
        thumb = bg.darker(125);
        hover = bg.darker(145);
        sideBg = bg;
        selBg = hover;
        selTxt = QColor(QStringLiteral("#000000"));
        dim = QColor(QStringLiteral("#777777"));
        chkBg = bg;
        chkImg = QStringLiteral("url(:/checkbox-checked-dark.svg)");
        upArrowImg = QStringLiteral("url(:/arrow-up-dark.svg)");
        downArrowImg = QStringLiteral("url(:/arrow-down-dark.svg)");
        radioImg = QStringLiteral("url(:/radio-dot-dark.svg)");
        btnBorder = thumb.darker(150);
    }
    chkCheckedBg = dark ? hover : bg.darker(120);
    if (!chkBgStr.isEmpty()) {
        QColor parsed(chkBgStr);
        if (parsed.isValid())
            chkCheckedBg = parsed;
    }
    QColor gutterBg = dark ? bg.darker(120) : bg.darker(105);
    QColor editorText;
    if (!txtStr.isEmpty()) {
        QColor parsed(txtStr);
        if (parsed.isValid())
            editorText = parsed;
    }
    if (!editorText.isValid())
        editorText = txt;
    QColor gutterText = mix(gutterBg, editorText, 30);

    return QStringLiteral(
        "QDialog { background-color: %2; }\n"
        "QScrollArea { background-color: %2; border: none; }\n"
        "QScrollArea > QWidget > QWidget { background-color: %2; }\n"
        "QGroupBox { color: %3; font-weight: bold; font-size: @FONT_SIZE@pt; border: 1px solid %4; margin-top: 14px; }\n"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 2px 8px; color: %3; font-weight: bold; font-size: @FONT_SIZE@pt; }\n"
        "QGroupBox::indicator { width: 14px; height: 14px; background-color: %12; border: 1px solid %4; }\n"
        "QGroupBox::indicator:checked { background-color: %13; border: 1px solid %13; image: %11; }\n"
        "QCheckBox { color: %3; font-size: @FONT_SIZE@pt; spacing: 6px; }\n"
        "QCheckBox:disabled { color: %9; }\n"
        "QCheckBox::indicator { width: 14px; height: 14px; background-color: %12; border: 1px solid %4; }\n"
        "QCheckBox::indicator:disabled { background-color: %10; border: 1px solid %10; }\n"
        "QCheckBox::indicator:checked { background-color: %13; border: 1px solid %13; image: %11; }\n"
        "QRadioButton { color: %3; font-size: @FONT_SIZE@pt; spacing: 6px; }\n"
        "QRadioButton:disabled { color: %9; }\n"
        "QRadioButton::indicator { width: 14px; height: 14px; background-color: %2; border: 1px solid %4; border-radius: 7px; }\n"
        "QRadioButton::indicator:disabled { background-color: %10; border: 1px solid %10; image: none; }\n"
        "QRadioButton::indicator:checked { background-color: %2; border: 1px solid %4; border-radius: 7px; image: %20; }\n"
        "QListWidget { background-color: %2; color: %3; border: none; }\n"
        "QListWidget::item:selected { background-color: %5; color: %6; }\n"
        "QListWidget::item:hover:!selected { background-color: %1; }\n"
        "QTextEdit { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: none; }\n"
        "QPlainTextEdit { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: none; }\n"
        "QPushButton { background-color: %4; color: %3; font-size: @FONT_SIZE@pt; border: 1px solid %19; padding: 4px 12px; }\n"
        "QPushButton:hover { background-color: %5; }\n"
        "QPushButton:disabled { color: %9; background-color: %2; border: 1px solid %2; }\n"
        "QLabel { color: %3; font-size: @FONT_SIZE@pt; }\n"
        "QLabel:disabled { color: %9; }\n"
        "QStatusBar { background-color: %2; color: %3; }\n"
        "#stats-label { color: %9; font-size: @FONT_SIZE@pt; }\n"
        "QMenuBar { background-color: %2; color: %3; border: none; }\n"
        "QMenuBar::item:selected { background-color: %5; }\n"
        "QMenu { background-color: %2; color: %3; border: 1px solid %4; }\n"
        "QMenu::item:selected { background-color: %5; color: %6; }\n"
        "QMenu::separator { background-color: %4; height: 1px; margin: 4px 8px; }\n"
        "#scriba-editor { padding: 0; margin: 0 !important; border: none !important; background-color: %7 !important; color: %8 !important; }\n"
        "#gutter { background-color: %17; color: %18; }\n"
        "#preferences-page-list { background-color: %10; color: %3; border: none; }\n"
        "#preferences-page-list::item { padding: 8px 4px; outline: none; }\n"
        "#preferences-page-list::item:selected { background-color: %5; color: %6; }\n"
        "#preferences-page-list::item:hover:!selected { background-color: %1; }\n"
        "#preferences-stylesheet-list { background-color: %10; color: %3; border: none; }\n"
        "#preferences-stylesheet-list::item { padding: 4px; outline: none; }\n"
        "#preferences-stylesheet-list::item:selected { background-color: %5; color: %6; }\n"
        "#preferences-stylesheet-list::item:hover:!selected { background-color: %1; }\n"
        "*[scribaPrefDim=\"true\"] { color: %9; }\n"
        "QSplitter::handle { background-color: %4; width: 1px; }\n"
        "QSplitter::handle:hover { background-color: %5; }\n"
        "#preferences-stylesheet-list QScrollBar:vertical, #preferences-stylesheet-list QScrollBar:horizontal { background: %2; }\n"
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
        "QLineEdit { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: 1px solid %4; padding: 2px 4px; }\n"
        "QLineEdit:disabled { background-color: %10; color: %9; border: 1px solid %10; }\n"
        "QSpinBox, QDoubleSpinBox { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: 1px solid %4; padding: 2px 4px; }\n"
        "QSpinBox:disabled, QDoubleSpinBox:disabled { background-color: %10; color: %9; border: 1px solid %10; }\n"
        "QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 20px; background-color: %10; border-left: 1px solid %4; border-bottom: 1px solid %4; }\n"
        "QSpinBox::up-button:disabled, QDoubleSpinBox::up-button:disabled { background-color: %10; }\n"
        "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover { background-color: %1; }\n"
        "QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed { background-color: %5; }\n"
        "QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 20px; background-color: %10; border-left: 1px solid %4; }\n"
        "QSpinBox::down-button:disabled, QDoubleSpinBox::down-button:disabled { background-color: %10; }\n"
        "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background-color: %1; }\n"
        "QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed { background-color: %5; }\n"
        "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: %14; width: 8px; height: 8px; }\n"
        "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: %15; width: 8px; height: 8px; }\n"
        "QComboBox { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: 1px solid %4; padding: 2px 4px; }\n"
        "QComboBox:disabled { background-color: %10; color: %9; border: 1px solid %10; }\n"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 22px; border-left: 1px solid %4; }\n"
        "QComboBox::drop-down:disabled { border-left: 1px solid %10; }\n"
        "QComboBox::down-arrow { image: %15; width: 8px; height: 8px; }\n"
        "QComboBox QAbstractItemView { background-color: %10; color: %3; font-size: @FONT_SIZE@pt; selection-background-color: %5; selection-color: %6; outline: none; }\n"
        "QTableWidget, QTableView { background-color: %2; color: %3; font-size: @FONT_SIZE@pt; border: 1px solid %4; gridline-color: %4; }\n"
        "QTableCornerButton::section { background-color: %2; border: 1px solid %4; }\n"
        "QHeaderView { background-color: %2; }\n"
        "QHeaderView::section { background-color: %2; color: %3; padding: 4px; border: 1px solid %4; font-weight: bold; }\n"
        "QTabBar { background-color: %2; border: none; }\n"
        "QTabBar::tab { background-color: %2; color: %3; padding: 6px 32px; border: none; border-bottom: 1px solid %4; border-top-left-radius: 6px; border-top-right-radius: 6px; }\n"
        "QTabBar::tab:selected { background-color: %7; color: %3; border: none; }\n"
        "QTabBar::tab:hover:!selected { background-color: %5; }\n"
        "QTabBar::close-button { subcontrol-origin: content; subcontrol-position: right; }\n"
        "\n"
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
        txtStr.isEmpty() ? txt.name() : txtStr, // %8 — editor text color from theme
        dim.name()    // %9 — dim text for stats label
    ).arg(
        sideBg.name()  // %10 — sidebar background
    ).arg(
        chkImg         // %11 — checkbox checked image
    ).arg(
        chkBg.name()   // %12 — checkbox indicator background
    ).arg(
        chkCheckedBg.name() // %13 — checkbox checked background
    ).arg(
        upArrowImg   // %14 — spinbox up arrow
    ).arg(
        downArrowImg // %15 — spinbox down arrow
    ).arg(
        gutterBg.name()   // %17 — line-number gutter background
    ).arg(
        gutterText.name() // %18 — line-number gutter text
    ).arg(
        btnBorder.name()  // %19 — push button border
    ).arg(
        radioImg         // %20 — radio checked dot
    ).replace(QStringLiteral("@FONT_SIZE@"), QString::number(uiFontSizePt));
}

ThemeColors themeColors(const QString &themeCss)
{
    auto extractBg = [&](const QString &selector) {
        QRegularExpression re(
            selector + R"(\s*\{[^}]*background(?:-color)?\s*:\s*([^;\}]+))"
        );
        auto it = re.globalMatch(themeCss);
        QString result;
        while (it.hasNext())
            result = it.next().captured(1).trimmed();
        return result;
    };

    auto extractColor = [&](const QString &selector) {
        QRegularExpression re(
            selector + R"(\s*\{(?:[^}]*;\s*)?\bcolor\s*:\s*([^;\}]+))"
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

    QColor txt(QStringLiteral("#333333"));
    if (!txtStr.isEmpty()) {
        QColor parsed(txtStr);
        if (parsed.isValid())
            txt = parsed;
    }

    return { bg, txt };
}

bool isDarkTheme(const QString &themeCss)
{
    return themeColors(themeCss).background.lightness() < 128;
}

QString renderOverlayCss(const QString &themeCss)
{
    ThemeColors tc = themeColors(themeCss);
    // Text blended 25% toward the background: "Rendering…" is paler than the
    // theme text so it reads as a transient notice rather than real content
    QColor text(tc.text.red() + (tc.background.red() - tc.text.red()) * 25 / 100,
                 tc.text.green() + (tc.background.green() - tc.text.green()) * 25 / 100,
                 tc.text.blue() + (tc.background.blue() - tc.text.blue()) * 25 / 100);
    return QString(
        "#scriba-rendering-overlay{"
        "position:fixed;top:0;left:0;right:0;bottom:0;z-index:2147483647;"
        "display:flex;align-items:center;justify-content:center;"
        "background:%1;color:%2;font:39px system-ui,sans-serif;letter-spacing:.04em;"
        "}"
    ).arg(tc.background.name(), text.name());
}

QString splitViewMaxWidthCss(int maxWidth)
{
    if (maxWidth <= 0)
        return QStringLiteral("body{max-width:none!important;margin:0!important}");
    return QStringLiteral("body{margin:0 auto!important;max-width:%1px!important}")
        .arg(maxWidth);
}

} // namespace CssUtils
