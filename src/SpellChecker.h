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

#include <QSet>
#include <QString>
#include <QStringList>

#include "stoppard/stoppard.h"

// Wraps the vendored stoppard spelling engine (vendor/stoppard). Owns the
// active base dictionary (en_US / en_GB), the per-user word dictionary and
// the imported word lists (plain .txt, one word per line). Bundled
// dictionaries are extracted from the qrc bundle on first use (stoppard needs
// real file paths, not qrc:// virtual ones); extracted copies carry a version
// marker so a stale config copy is superseded to .bak when a bundled
// dictionary changes.
class SpellChecker
{
public:
    SpellChecker();
    ~SpellChecker();

    bool loadLanguage(const QString &language);
    QString language() const { return m_language; }
    bool isLoaded() const { return !m_language.isEmpty(); }
    // Dialect for the spelling allowances (Canadian/Māori lists). Accepts the
    // same strings as StoppardEngine ("American", "British", "Australian",
    // "Indian", "Canadian", "New Zealand").
    void setDialect(const QString &dialect);

    bool checkWord(const QString &word);
    QStringList suggestions(const QString &word);
    // How many times the engine has actually been reconfigured (dictionaries
    // reloaded and lookup tables rebuilt). Exposed for tests to assert that
    // reapplying an unchanged configuration is a no-op.
    quint64 configLoads() const { return m_configLoads; }

    void addToUserDictionary(const QString &word);
    void removeFromUserDictionary(const QString &word);
    QStringList userWords() const;

    // "Ignore always" list: like the user dictionary (persisted, loaded every
    // session) but managed separately so the two sets stay distinct. Also
    // persisted with a count-header format in ignored.dic.
    void addToIgnored(const QString &word);
    void removeFromIgnored(const QString &word);
    QStringList ignoredWords() const;
    // File-level accessors (usable without a loaded language) so the
    // Preferences dialog can manage the ignored list independently.
    static QStringList readIgnoredWords();
    static void writeIgnoredWords(const QStringList &words);

    // Corpus-scoped word sets. While a corpus is active these replace
    // (override) or union (merge) with the global user.dic/ignored.dic sets
    // (§22 of the corpus plan). With no corpus words set — or with merge
    // enabled — the global sets always apply, so a plain editor keeps its user
    // dictionary even in the default override mode.
    void setCorpusWords(const QStringList &words);
    void setCorpusIgnored(const QStringList &words);
    void setCorpusMerge(bool merge);
    void addCorpusWord(const QString &word);
    void removeCorpusWord(const QString &word);
    void addCorpusIgnored(const QString &word);
    QStringList corpusWords() const;
    QStringList corpusIgnored() const;

    // The base language a dialect selects under the "follow dialect" setting.
    static QString defaultLanguageForDialect(const QString &dialect);

    static QStringList availableLanguages();
    static QString configDictDir();
    static bool isBundledLanguage(const QString &language);
    // Imported dictionaries (plain word lists): copied into the config dir,
    // merged into the active dictionary set; language-independent (§19.3).
    static QStringList importedDictionaries();
    static QString installDictionary(const QString &txtPath);
    static bool removeDictionary(const QString &base);

    // File/settings-level accessors (usable without a loaded language) so the
    // Preferences dialog can manage the list independently of any editor.
    static QStringList readUserDictionaryWords();
    static void writeUserDictionaryWords(const QStringList &words);

    // Parses user-supplied word list text (one word per line) into trimmed,
    // non-empty words. A leading integer count line (user.dic header) is
    // skipped so such files import cleanly.
    static QStringList parseWordList(const QString &text);

private:
    void applyEngineConfig();
    void loadUserDictionary();
    void loadIgnoredWords();
    QStringList importedWords() const;

    stoppard::Engine m_engine;
    QString m_language;
    QString m_dialect = QStringLiteral("American");
    QString m_configKey;
    quint64 m_configLoads = 0;
    QSet<QString> m_userWords;
    QSet<QString> m_ignoredWords;
    QSet<QString> m_corpusWords;
    QSet<QString> m_corpusIgnored;
    bool m_corpusMerge = false;
};