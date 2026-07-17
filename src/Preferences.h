#ifndef PREFERENCES_H
#define PREFERENCES_H

namespace Preferences {
    constexpr const char *CssFiles = "cssFiles";
    constexpr const char *ActiveCssFile = "activeCssFile";
    constexpr const char *PrintCssFiles = "printCssFiles";
    constexpr const char *ActivePrintCssFile = "activePrintCssFile";
    constexpr const char *FirstRun = "firstRun";
    constexpr const char *ReopenLastFile = "reopenLastFile";
    constexpr const char *SyncScroll = "syncScroll";
    constexpr const char *LastOpenedFile = "lastOpenedFile";
    constexpr const char *PreviewState = "previewState";
    constexpr const char *EditorOnLeft = "editorOnLeft";
    constexpr const char *TableStriping = "tableStriping";

    constexpr const char *TableStripeCss = "tr:nth-child(even){background-color:transparent}";
    constexpr const char *TableStripePdfCss = "tr:nth-child(even),tr:nth-child(even) td{background-color:transparent !important}";
}

#endif
