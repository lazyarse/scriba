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
#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>

#include "StaticHelpers.h"

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
    constexpr const char *EmojiCompletionLimit = "emojiCompletionLimit";
    constexpr const char *CentreSingleViewContent = "centreSingleViewContent";
    constexpr const char *CentreSingleViewWidth = "centreSingleViewWidth";
    constexpr const char *SplitViewEditorMaxWidth = "splitViewEditorMaxWidth";
    constexpr const char *SplitViewPreviewMaxWidth = "splitViewPreviewMaxWidth";

    constexpr const char *AutoSaveOnExit = "autoSaveOnExit";
    constexpr const char *AutoSaveInterval = "autoSaveInterval";
    constexpr const char *FileCompletionLimit = "fileCompletionLimit";
    constexpr const char *FileAutoComplete = "fileAutoComplete";
    constexpr const char *LanguageAutoComplete = "languageAutoComplete";
    constexpr const char *AutoCorrectEnabled = "autoCorrectEnabled";
    constexpr const char *AutoCorrectPairs = "autoCorrectPairs";

    constexpr const char *AutoAlignTables = "autoAlignTables";

    constexpr const char *TypographyQuotes = "typographyQuotes";
    constexpr const char *TypographyDashes = "typographyDashes";
    constexpr const char *TypographyEllipsis = "typographyEllipsis";
    constexpr const char *TypographyMultiplication = "typographyMultiplication";
    constexpr const char *TypographyDegreeFractionPrime = "typographyDegreeFractionPrime";
    constexpr const char *TypographyNbsp = "typographyNbsp";
    constexpr const char *TypographySymbols = "typographySymbols";

    constexpr const char *HeavyRenderDelay = "heavyRenderDelay";
    constexpr int DefaultHeavyRenderDelay = Debounce::HeavyRender;
    constexpr const char *PreviewUpdateDelay = "previewUpdateDelay";
    constexpr int DefaultPreviewUpdateDelay = Debounce::PreviewUpdate;

    constexpr const char *HardSoftBreaks = "hardSoftBreaks";

    constexpr const char *ConfigVersion = "configVersion";
    constexpr int CurrentConfigVersion = 1;

    constexpr const char *EditorFontFamily = "editorFontFamily";
    constexpr const char *EditorFontSize = "editorFontSize";
    constexpr int DefaultEditorFontSize = 12;
    constexpr const char *EditorLineHeight = "editorLineHeight";
    constexpr int DefaultEditorLineHeight = 125;
    constexpr const char *EditorPadding = "editorPadding";
    constexpr const char *EditorBgColor = "editorBgColor";
    constexpr const char *EditorFontColor = "editorFontColor";
    constexpr const char *EditorColorOverride = "editorColorOverride";
    constexpr const char *EditorCaretWidth = "editorCaretWidth";
    constexpr int DefaultEditorCaretWidth = 1;

    // Editor line wrapping. EditorWrapEnabled is the master switch (the View
    // menu's "Wrap Text" toggle flips this). While enabled, EditorWrapMode
    // selects the style: "window" wraps at the pane/centred width, "column"
    // wraps at EditorWrapColumn characters (which then becomes the editor's
    // effective max width for centring).
    constexpr const char *EditorWrapEnabled = "editorWrapEnabled";
    constexpr const char *EditorWrapMode = "editorWrapMode";
    constexpr int DefaultEditorWrapColumn = 80;
    constexpr const char *EditorWrapColumn = "editorWrapColumn";

    constexpr const char *UiFontSize = "uiFontSize";
    constexpr int DefaultUiFontSize = 10;

    constexpr const char *ShowLineNumbers = "showLineNumbers";
    constexpr const char *GutterColorOverride = "gutterColorOverride";
    constexpr const char *GutterBgColor = "gutterBgColor";
    constexpr const char *GutterTextColor = "gutterTextColor";

    constexpr const char *PdfShowHeader = "pdfShowHeader";

    constexpr const char *SpellCheckEnabled = "spellCheckEnabled";
    constexpr const char *GrammarCheckEnabled = "grammarCheckEnabled";
    constexpr const char *LinkCheckEnabled = "linkCheckEnabled";
    constexpr const char *MarkdownCheckEnabled = "markdownCheckEnabled";
    // Per-check toggles for the real-time (in-editor) markdown-consistency
    // underlines. Independent of the Validation Report dialog's own selection.
    // Each defaults to enabled; only meaningful while MarkdownCheckEnabled is on.
    constexpr const char *MarkdownCheckHeadingLevelSkip = "mdRealHeadingLevelSkip";
    constexpr const char *MarkdownCheckDuplicateHeading = "mdRealDuplicateHeading";
    constexpr const char *MarkdownCheckTrailingWhitespace = "mdRealTrailingWhitespace";
    constexpr const char *MarkdownCheckConsecutiveBlankLines = "mdRealBlankLines";
    constexpr const char *MarkdownCheckOverlongLine = "mdRealOverlongLine";
    constexpr const char *MarkdownCheckHashNoSpace = "mdRealHashNoSpace";
    constexpr const char *MarkdownCheckFootnoteReference = "mdRealFootnote";
    constexpr const char *DictionaryLanguage = "dictionaryLanguage";
    constexpr const char *GrammarDialect = "grammarDialect";
    constexpr const char *UnderlineColorOverride = "underlineColorOverride";
    constexpr const char *SpellUnderlineColor = "spellUnderlineColor";
    constexpr const char *GrammarUnderlineColor = "grammarUnderlineColor";
    constexpr const char *LinkUnderlineColor = "linkUnderlineColor";
    constexpr const char *MarkdownUnderlineColor = "markdownUnderlineColor";

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
    constexpr const char *HideCodeLangCss = "pre[data-lang]::before{content:none}";

    constexpr const char *ShowCodeLangPreview = "showCodeLangPreview";
    constexpr const char *ShowCodeLangExport = "showCodeLangExport";

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

    // "typo=replacement" pairs shipped by default. Users may edit, remove or add
    // to these; the Preferences dialog's Restore Defaults button merges them back.
    inline QStringList defaultAutoCorrectPairs()
    {
        return {
            QStringLiteral("teh=the"),
            QStringLiteral("hte=the"),
            QStringLiteral("adn=and"),
            QStringLiteral("nad=and"),
            QStringLiteral("waht=what"),
            QStringLiteral("taht=that"),
            QStringLiteral("wih=with"),
            QStringLiteral("fo=of"),
            QStringLiteral("ot=to"),
            QStringLiteral("tehre=there"),
            QStringLiteral("theyre=they're"),
            QStringLiteral("dont=don't"),
            QStringLiteral("cant=can't"),
            QStringLiteral("wont=won't"),
            QStringLiteral("im=I'm"),
            QStringLiteral("ive=I've"),
            QStringLiteral("yuo=you"),
            QStringLiteral("recieve=receive"),
            QStringLiteral("seperate=separate"),
            QStringLiteral("definately=definitely"),
            QStringLiteral("occured=occurred"),
            QStringLiteral("ocassion=occasion"),
            QStringLiteral("alot=a lot"),
        };
    }

    // Migrate a pre-versioning config (no "configVersion" key) forward to the
    // current schema: carry over renamed keys, drop removed options so stale
    // values can never affect behaviour again, and stamp the version. Keys that
    // were never known to Scriba are left untouched.
    inline void migrateSettings(QSettings &settings)
    {
        if (settings.value(ConfigVersion, 0).toInt() >= CurrentConfigVersion)
            return;

        if (!settings.contains(ReopenLastSession) && settings.contains("reopenLastFile"))
            settings.setValue(ReopenLastSession, settings.value("reopenLastFile"));
        settings.remove("reopenLastFile");

        static const QStringList removedKeys = {
            QStringLiteral("darkMode"),
            QStringLiteral("editorOnLeft"),
            QStringLiteral("showFoldIcons"),
            QStringLiteral("firstRun"),
            QStringLiteral("printCssFiles"),
            QStringLiteral("activePrintCssFile"),
            QStringLiteral("cssDirectory"),
            QStringLiteral("enabledCssFiles"),
            QStringLiteral("EditorFont"),
            QStringLiteral("editorFont"),
        };
        for (const QString &key : removedKeys)
            settings.remove(key);

        settings.setValue(ConfigVersion, CurrentConfigVersion);
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

