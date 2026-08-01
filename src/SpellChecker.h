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

private:
    bool findDictionaryFiles(const QString &language, QString &aff, QString &dic) const;
    void loadUserDictionary();
    QString userDictPath() const;

    std::unique_ptr<Hunspell> m_hunspell;
    QString m_language;
    QSet<QString> m_userWords;
};
