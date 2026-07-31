#pragma once

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
    constexpr const char *SplitViewEditorMaxWidth = "splitViewEditorMaxWidth";
    constexpr const char *SplitViewPreviewMaxWidth = "splitViewPreviewMaxWidth";

    constexpr const char *AutoSaveOnExit = "autoSaveOnExit";
    constexpr const char *AutoSaveInterval = "autoSaveInterval";
    constexpr const char *FileCompletionLimit = "fileCompletionLimit";
    constexpr const char *FileAutoComplete = "fileAutoComplete";

    constexpr const char *EditorFontFamily = "editorFontFamily";
    constexpr const char *EditorFontSize = "editorFontSize";
    constexpr int DefaultEditorFontSize = 12;
    constexpr const char *EditorLineHeight = "editorLineHeight";
    constexpr int DefaultEditorLineHeight = 125;
    constexpr const char *EditorPadding = "editorPadding";
    constexpr const char *EditorBgColor = "editorBgColor";
    constexpr const char *EditorFontColor = "editorFontColor";
    constexpr const char *EditorColorOverride = "editorColorOverride";

    constexpr const char *ShowLineNumbers = "showLineNumbers";
    constexpr const char *ShowFoldIcons = "showFoldIcons";
    constexpr const char *GutterColorOverride = "gutterColorOverride";
    constexpr const char *GutterBgColor = "gutterBgColor";
    constexpr const char *GutterTextColor = "gutterTextColor";

    constexpr const char *PdfShowHeader = "pdfShowHeader";

    constexpr const char *StripPreviewScripts = "stripPreviewScripts";
    constexpr const char *StripExportScripts = "stripExportScripts";
    constexpr const char *BlockRawHtmlPreview = "blockRawHtmlPreview";
    constexpr const char *BlockRawHtmlExport = "blockRawHtmlExport";
    constexpr const char *EnableCspPreview = "enableCspPreview";
    constexpr const char *EnableCspExport = "enableCspExport";
    constexpr const char *ReadabilityFormula = "readabilityFormula";
    constexpr const char *StatusBarMetrics = "statusBarMetrics";
    constexpr const char *WordsPerSecond = "wordsPerSecond";
    constexpr const char *SpeakingWpm = "speakingWpm";

    constexpr const char *TableStripeCss = "tr:nth-child(even){background-color:transparent}";
    constexpr const char *TableStripePdfCss = "tr:nth-child(even),tr:nth-child(even) td{background-color:transparent !important}";

    enum class EmojiRendering { Bw, Color };

    enum class Formula {
        FleschKincaid,
        ColemanLiau,
        GunningFog,
        Smog,
        ARI
    };

    inline Formula formulaFromString(const QString &s)
    {
        if (s == QLatin1String("coleman-liau")) return Formula::ColemanLiau;
        if (s == QLatin1String("gunning-fog")) return Formula::GunningFog;
        if (s == QLatin1String("smog")) return Formula::Smog;
        if (s == QLatin1String("ari")) return Formula::ARI;
        return Formula::FleschKincaid;
    }

    inline QString formulaToString(Formula f)
    {
        switch (f) {
        case Formula::ColemanLiau: return QStringLiteral("coleman-liau");
        case Formula::GunningFog: return QStringLiteral("gunning-fog");
        case Formula::Smog: return QStringLiteral("smog");
        case Formula::ARI: return QStringLiteral("ari");
        default: return QStringLiteral("flesch-kincaid");
        }
    }

    inline const char *formulaLabel(Formula f)
    {
        switch (f) {
        case Formula::ColemanLiau: return "CL";
        case Formula::GunningFog: return "Fog";
        case Formula::Smog: return "SMOG";
        case Formula::ARI: return "ARI";
        default: return "FK";
        }
    }

    inline EmojiRendering emojiRenderingFromString(const QString &s)
    {
        return s == QLatin1String("color") ? EmojiRendering::Color : EmojiRendering::Bw;
    }

    inline QString emojiRenderingToString(EmojiRendering mode)
    {
        return mode == EmojiRendering::Color ? QStringLiteral("color") : QStringLiteral("bw");
    }
}

namespace Security {
    constexpr const char *CspHeader =
        "default-src 'self' qrc:;"
        "script-src 'self' qrc: 'unsafe-inline' 'unsafe-eval';"
        "style-src 'self' qrc: 'unsafe-inline';"
        "img-src 'self' qrc: data: file:;"
        "font-src 'self' qrc: data:;"
        "connect-src 'self' qrc: data: blob:;";
}

