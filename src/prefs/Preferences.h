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
    constexpr const char *ReopenLastCorpus = "reopenLastCorpus";
    // When a corpus is reopened, restore each document's saved cursor + viewport
    // position (off = reset to the top-left of the document). Positions are still
    // always saved to disk.
    constexpr const char *RestorePositions = "restorePositions";
    constexpr const char *OnExitCorpusData = "onExitCorpusData";
    constexpr const char *LastCorpusPath = "lastCorpusPath";
    constexpr const char *RecentCorpora = "recentCorpora";
    constexpr int MaxRecentCorpora = 5;
    constexpr const char *CorpusDictionaryMode = "corpusDictionaryMode";
    constexpr const char *CorpusExternalEditPolicy = "corpusExternalEditPolicy";
    constexpr const char *CorpusLinkRewritePolicy = "corpusLinkRewritePolicy";
    constexpr const char *CorpusLinkRewriteScope = "corpusLinkRewriteScope";
    // How untitled corpus documents are persisted. "embed" stores their content
    // inside the .scriba file (the legacy default); "prompt" asks the user to
    // save each one to a real file first (the corpus then stores it by path)
    // and aborts the corpus save / window close if the user cancels.
    constexpr const char *CorpusUnsavedDocs = "corpusUnsavedDocs";
    constexpr const char *CorpusExternalExportDirName = "corpusExternalExportDirName";
    // Corpus Files sidecar panel (View menu). Whether the dock is shown when a
    // corpus is open; the dock hides itself when no corpus is open.
    constexpr const char *ShowCorpusFilesPanel = "showCorpusFilesPanel";
    // File-based corpus Table of Contents. The template is a seed used only
    // when creating a corpus's toc.md; each corpus's own toc.md is its saved
    // template, so changing the preference never rewrites existing files.
    constexpr const char *CorpusTocFileName = "corpusTocFileName";
    constexpr const char *CorpusTocTemplate = "corpusTocTemplate";
    constexpr const char *SyncScroll = "syncScroll";
    constexpr const char *LastOpenedFile = "lastOpenedFile";
    constexpr const char *LastCursorBlock = "lastCursorBlock";
    constexpr const char *LastCursorColumn = "lastCursorColumn";
    constexpr const char *LastScrollTop = "lastScrollTop";
    constexpr const char *PreviewState = "previewState";
    constexpr const char *PreviewShowPageBreaks = "previewShowPageBreaks";
    constexpr const char *TableStriping = "tableStriping";
    constexpr const char *EmojiMode = "emojiMode";
    constexpr const char *EmojiAutoComplete = "emojiAutoComplete";
    constexpr const char *EmojiCompletionLimit = "emojiCompletionLimit";
    constexpr const char *CentreSingleViewContent = "centreSingleViewContent";
    constexpr const char *TabBarAlwaysShow = "tabBarAlwaysShow";
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
    // Number of spaces around each cell's content when formatting markdown
    // tables (Editor → Tables → "Cell padding").
    constexpr const char *TablePadding = "tablePadding";
    constexpr int DefaultTablePadding = 1;

    constexpr const char *TypographyQuotes = "typographyQuotes";
    constexpr const char *TypographyDashes = "typographyDashes";
    constexpr const char *TypographyEllipsis = "typographyEllipsis";
    constexpr const char *TypographyMultiplication = "typographyMultiplication";
    constexpr const char *TypographyDegreeFractionPrime = "typographyDegreeFractionPrime";
    constexpr const char *TypographyNbsp = "typographyNbsp";
    constexpr const char *TypographySymbols = "typographySymbols";
    constexpr const char *TypographyArrows = "typographyArrows";

    constexpr const char *HeavyRenderDelay = "heavyRenderDelay";
    constexpr int DefaultHeavyRenderDelay = Debounce::HeavyRender;
    constexpr const char *PreviewUpdateDelay = "previewUpdateDelay";
    constexpr int DefaultPreviewUpdateDelay = Debounce::LightRender;

    constexpr const char *HardSoftBreaks = "hardSoftBreaks";

    // Ordered-list numbering style in the preview and exports. The source
    // keeps whatever delimiter the user typed; this only picks the rendered
    // format. See MdRenderer::OrderedListStyle.
    constexpr const char *OrderedListMarker = "orderedListMarker";

    // Where imported DOCX images are written: next to the document, into a
    // configured folder, to the system temp dir (until the doc is saved), or
    // ask each time.
    constexpr const char *ImportImageLocation = "importImageLocation";
    constexpr const char *ImportImageDir = "importImageDir";

    constexpr const char *ConfigVersion = "configVersion";
    constexpr int CurrentConfigVersion = 2;

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

    // Printing (PrintOptions model): defaults mirror DR-2 (all "on"/no split),
    // so the all-defaults buildCss() output is empty.
    constexpr const char *PrintCodeSplit = "printCodeSplit";            // "never"|"small"|"large"
    constexpr const char *PrintKeepTables = "printKeepTables";          // default true
    constexpr const char *PrintKeepHeadings = "printKeepHeadings";      // default true
    constexpr const char *PrintKeepFigures = "printKeepFigures";        // default true
    constexpr const char *PrintOrphanControl = "printOrphanControl";    // default true
    constexpr const char *PrintPageMargin = "printPageMargin";          // "" = base default (15mm)
    constexpr const char *PrintPageSize = "printPageSize";              // "" = default

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

    inline const char *defaultOrderedListMarker()
    {
        return "decimal";
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

        if (!settings.contains(ReopenLastCorpus) && settings.contains("reopenLastFile"))
            settings.setValue(ReopenLastCorpus, settings.value("reopenLastFile"));
        settings.remove("reopenLastFile");

        // No backward compatibility with the former "session" feature: old
        // session keys are removed and their values discarded.
        settings.remove("reopenLastSession");
        settings.remove("sessionData");
        settings.remove("lastSessionName");

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

