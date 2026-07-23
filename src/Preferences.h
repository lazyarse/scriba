#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QString>

namespace Preferences {
    constexpr const char *CssFiles = "cssFiles";
    constexpr const char *ActiveCssFile = "activeCssFile";
    constexpr const char *FirstRun = "firstRun";
    constexpr const char *ReopenLastFile = "reopenLastFile";
    constexpr const char *SyncScroll = "syncScroll";
    constexpr const char *LastOpenedFile = "lastOpenedFile";
    constexpr const char *LastCursorBlock = "lastCursorBlock";
    constexpr const char *LastCursorColumn = "lastCursorColumn";
    constexpr const char *LastScrollTop = "lastScrollTop";
    constexpr const char *PreviewState = "previewState";
    constexpr const char *TableStriping = "tableStriping";
    constexpr const char *EmojiMode = "emojiMode";
    constexpr const char *EmojiAutoComplete = "emojiAutoComplete";

    constexpr const char *AutoSaveOnExit = "autoSaveOnExit";
    constexpr const char *AutoSaveInterval = "autoSaveInterval";
    constexpr const char *FileCompletionLimit = "fileCompletionLimit";

    constexpr const char *PdfShowHeader = "pdfShowHeader";

    constexpr const char *TableStripeCss = "tr:nth-child(even){background-color:transparent}";
    constexpr const char *TableStripePdfCss = "tr:nth-child(even),tr:nth-child(even) td{background-color:transparent !important}";

    enum class EmojiRendering { Bw, Color };

    inline EmojiRendering emojiRenderingFromString(const QString &s)
    {
        return s == QLatin1String("color") ? EmojiRendering::Color : EmojiRendering::Bw;
    }

    inline QString emojiRenderingToString(EmojiRendering mode)
    {
        return mode == EmojiRendering::Color ? QStringLiteral("color") : QStringLiteral("bw");
    }
}

#endif
