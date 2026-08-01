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
#include <memory>

class Hunspell;

// Wraps the vendored hunspell engine. Owns the active language dictionary and
// the per-user word dictionary. Bundled en_US/en_GB dictionaries are extracted
// from the qrc bundle on first use (hunspell needs real file paths, not qrc://
// virtual ones).
class SpellChecker
{
public:
    SpellChecker();
    ~SpellChecker();

    bool loadLanguage(const QString &language);
    QString language() const { return m_language; }
    bool isLoaded() const { return m_hunspell != nullptr; }

    bool checkWord(const QString &word);
    QStringList suggestions(const QString &word);

    void addToUserDictionary(const QString &word);
    void removeFromUserDictionary(const QString &word);
    QStringList userWords() const;

    static QStringList availableLanguages();
    static QString configDictDir();
    static bool isBundledLanguage(const QString &language);
    static QString installDictionary(const QString &affOrDicPath);
    static bool removeDictionary(const QString &language);

    // File/settings-level accessors (usable without a loaded language) so the
    // Preferences dialog can manage the list independently of any editor.
    static QStringList readUserDictionaryWords();
    static void writeUserDictionaryWords(const QStringList &words);

    // Parses user-supplied word list text (one word per line) into trimmed,
    // non-empty words. A leading integer count line (hunspell user.dic header)
    // is skipped so such files import cleanly.
    static QStringList parseWordList(const QString &text);

private:
    bool findDictionaryFiles(const QString &language, QString &aff, QString &dic) const;
    void loadUserDictionary();
    QString userDictPath() const;

    std::unique_ptr<Hunspell> m_hunspell;
    QString m_language;
    QSet<QString> m_userWords;
};
