#ifndef PREFERENCES_H
#define PREFERENCES_H

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

    constexpr const char *AutoSaveOnExit = "autoSaveOnExit";
    constexpr const char *AutoSaveInterval = "autoSaveInterval";
    constexpr const char *FileCompletionLimit = "fileCompletionLimit";

    constexpr const char *PdfShowHeader = "pdfShowHeader";

    constexpr const char *TableStripeCss = "tr:nth-child(even){background-color:transparent}";
    constexpr const char *TableStripePdfCss = "tr:nth-child(even),tr:nth-child(even) td{background-color:transparent !important}";
}

#endif
