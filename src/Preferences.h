#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QString>

namespace Preferences {
    constexpr const char *CssFiles = "cssFiles";
    constexpr const char *ActiveCssFile = "activeCssFile";
    constexpr const char *ReopenLastSession = "reopenLastSession";
    constexpr const char *SessionData = "sessionData";
    constexpr const char *LastSessionName = "lastSessionName";
    constexpr const char *SyncScroll = "syncScroll";
    constexpr const char *LastOpenedFile = "lastOpenedFile";
    constexpr const char *LastCursorBlock = "lastCursorBlock";
    constexpr const char *LastCursorColumn = "lastCursorColumn";
    constexpr const char *LastScrollTop = "lastScrollTop";
    constexpr const char *PreviewState = "previewState";
    constexpr const char *TableStriping = "tableStriping";
    constexpr const char *EmojiMode = "emojiMode";
    constexpr const char *EmojiAutoComplete = "emojiAutoComplete";
    constexpr const char *CentreSingleViewContent = "centreSingleViewContent";
    constexpr const char *CentreSingleViewWidth = "centreSingleViewWidth";

    constexpr const char *AutoSaveOnExit = "autoSaveOnExit";
    constexpr const char *AutoSaveInterval = "autoSaveInterval";
    constexpr const char *FileCompletionLimit = "fileCompletionLimit";
    constexpr const char *FileAutoComplete = "fileAutoComplete";

    constexpr const char *EditorFontFamily = "editorFontFamily";
    constexpr const char *EditorFontSize = "editorFontSize";
    constexpr const char *EditorLineHeight = "editorLineHeight";
    constexpr const char *EditorPadding = "editorPadding";
    constexpr const char *EditorBgColor = "editorBgColor";
    constexpr const char *EditorFontColor = "editorFontColor";
    constexpr const char *EditorColorOverride = "editorColorOverride";

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
