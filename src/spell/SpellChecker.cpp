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
#include "SpellChecker.h"
#include "css/CssUtils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <vector>

namespace {

constexpr const char *BundledFiles[] = {"en-US.txt", "en-GB.txt", "maori-nz.txt",
                                        "canadian-en.txt", "keyboard-en-GB.txt",
                                        "freq-en.txt"};

QString bundledDictDir()
{
    return SpellChecker::configDictDir() + "/bundled";
}

// Supersede a stale extracted copy to `<file>.bak`, mirroring CssLoader.
void supersedeStaleFile(const QString &path)
{
    QFile::remove(path + ".bak");
    if (!QFile::rename(path, path + ".bak"))
        QFile::remove(path);
}

// The version marker that guards a config-dir copy against a changed bundled
// dictionary. A '#' first line is skipped by stoppard's word-list reader.
QString dictMarker(const QString &hash)
{
    return QStringLiteral("# scriba-dict-version: %1\n").arg(hash);
}

QString extractDictMarker(const QString &text)
{
    static const QRegularExpression re(
        QStringLiteral("^# scriba-dict-version: ([0-9a-f]{64})\\s*$"));
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch())
            return m.captured(1);
    }
    return {};
}

// Extract `name` from the qrc bundle to the config dir, honouring a version
// marker: a config copy is used only while its marker matches the current
// bundle hash; otherwise it is superseded to .bak and the bundle is written
// fresh. Returns false only when the resource itself is missing.
bool copyBundledIfNeeded(const QString &name)
{
    QDir().mkpath(bundledDictDir());
    QFile src(":/dictionaries/" + name);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    const QByteArray content = src.readAll();
    const QString expected = QString::fromLatin1(
        QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
    const QString target = bundledDictDir() + "/" + name;

    QFile dst(target);
    if (dst.open(QIODevice::ReadOnly)) {
        if (extractDictMarker(QString::fromUtf8(dst.readAll())) == expected)
            return true;
        supersedeStaleFile(target);
    }
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    out.write(dictMarker(expected).toUtf8());
    out.write(content);
    return true;
}

// Restrict dictionary base names to letters/digits/_/- and refuse the reserved
// "user"/"bundled" names and the bundled dictionary file names so an imported
// list can never collide with the user word list or the bundled copies.
bool isSafeDictionaryBase(const QString &base)
{
    if (base.isEmpty() || base == QLatin1String(".") || base == QLatin1String(".."))
        return false;
    if (base == QLatin1String("user") || base == QLatin1String("bundled"))
        return false;
    for (QChar c : base) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('-'))
            return false;
    }
    return true;
}

// Bases of the bundled dictionary files (extracted copies live in the
// bundled/ subdir, so a config-dir file with one of these names would never
// be used — refuse them as import/remove targets).
bool isBundledFileName(const QString &base)
{
    return base == QLatin1String("en-US") || base == QLatin1String("en-GB")
        || base == QLatin1String("maori-nz") || base == QLatin1String("canadian-en")
        || base == QLatin1String("keyboard-en-GB") || base == QLatin1String("freq-en");
}

bool copyFileTo(const QString &src, const QString &dst)
{
    QFile s(src);
    if (!s.open(QIODevice::ReadOnly))
        return false;
    QFile d(dst);
    if (!d.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    d.write(s.readAll());
    return true;
}

} // namespace

SpellChecker::SpellChecker() = default;

SpellChecker::~SpellChecker() = default;

QString SpellChecker::configDictDir()
{
    return CssUtils::scribaConfigDir() + "/dictionaries";
}

// Reads a count-header word list (user.dic / ignored.dic format).
QStringList readCountHeaderList(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return {};

    bool ok = false;
    int count = lines.first().trimmed().toInt(&ok);
    if (!ok || count <= 0)
        return {};

    QStringList words;
    for (int i = 1; i < lines.size() && i <= count; ++i) {
        QString word = lines.at(i).trimmed();
        if (!word.isEmpty())
            words << word;
    }
    return words;
}

// Writes a count-header word list (user.dic / ignored.dic format).
void writeCountHeaderList(const QString &path, const QStringList &words)
{
    QStringList sorted = words;
    sorted.sort();
    sorted.removeDuplicates();

    QStringList lines;
    lines << QString::number(sorted.size());
    for (const QString &w : sorted)
        lines << w;
    QDir().mkpath(SpellChecker::configDictDir());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.write(lines.join('\n').toUtf8());
        file.write("\n");
    }
}

QStringList SpellChecker::readUserDictionaryWords()
{
    return readCountHeaderList(configDictDir() + "/user.dic");
}

void SpellChecker::writeUserDictionaryWords(const QStringList &words)
{
    writeCountHeaderList(configDictDir() + "/user.dic", words);
}

QStringList SpellChecker::readIgnoredWords()
{
    return readCountHeaderList(configDictDir() + "/ignored.dic");
}

void SpellChecker::writeIgnoredWords(const QStringList &words)
{
    writeCountHeaderList(configDictDir() + "/ignored.dic", words);
}

QStringList SpellChecker::parseWordList(const QString &text)
{
    QStringList words;
    bool countHeaderSeen = false;
    const QStringList lines = text.split('\n');
    for (QString line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!countHeaderSeen) {
            countHeaderSeen = true;
            bool ok = false;
            trimmed.toInt(&ok);
            if (ok)
                continue;
        }
        words << trimmed;
    }
    return words;
}

QString SpellChecker::defaultLanguageForDialect(const QString &dialect)
{
    if (dialect == QStringLiteral("British") || dialect == QStringLiteral("Australian")
        || dialect == QStringLiteral("Indian") || dialect == QStringLiteral("New Zealand"))
        return QStringLiteral("en_GB");
    return QStringLiteral("en_US");   // American (the default) / Canadian / unknown
}

QStringList SpellChecker::availableLanguages()
{
    for (const char *file : BundledFiles)
        copyBundledIfNeeded(QLatin1String(file));
    return {QStringLiteral("en_US"), QStringLiteral("en_GB")};
}

bool SpellChecker::isBundledLanguage(const QString &language)
{
    return language == QLatin1String("en_US") || language == QLatin1String("en_GB");
}

QStringList SpellChecker::importedDictionaries()
{
    QStringList lists;
    QDir dir(configDictDir());
    for (const QString &txt : dir.entryList({QStringLiteral("*.txt")}, QDir::Files)) {
        const QString base = QFileInfo(txt).completeBaseName();
        if (base.isEmpty() || base == QLatin1String("user"))
            continue;
        if (isBundledFileName(base) || !isSafeDictionaryBase(base))
            continue;
        lists << base;
    }
    lists.sort();
    return lists;
}

QString SpellChecker::installDictionary(const QString &txtPath)
{
    QFileInfo info(txtPath);
    if (!info.exists() || !info.isFile())
        return {};
    if (info.suffix().toLower() != QLatin1String("txt"))
        return {};
    const QString base = info.completeBaseName();
    if (isBundledFileName(base) || !isSafeDictionaryBase(base))
        return {};

    QFile src(txtPath);
    if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QStringList words = parseWordList(QString::fromUtf8(src.readAll()));
    if (words.isEmpty())
        return {};   // a word list with no words is not a dictionary

    QDir().mkpath(configDictDir());
    const QString dst = configDictDir() + "/" + base + ".txt";
    if (QFileInfo::exists(dst))
        return base;   // idempotent reinstall

    if (!copyFileTo(txtPath, dst))
        return {};
    return base;
}

bool SpellChecker::removeDictionary(const QString &base)
{
    if (isBundledFileName(base) || !isSafeDictionaryBase(base))
        return false;
    return QFile::remove(configDictDir() + "/" + base + ".txt");
}

QStringList SpellChecker::importedWords() const
{
    QStringList words;
    for (const QString &base : importedDictionaries()) {
        QFile file(configDictDir() + "/" + base + ".txt");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        words << parseWordList(QString::fromUtf8(file.readAll()));
    }
    return words;
}

bool SpellChecker::loadLanguage(const QString &language)
{
    if (language != QLatin1String("en_US") && language != QLatin1String("en_GB"))
        return false;
    for (const char *name : BundledFiles) {
        if (!copyBundledIfNeeded(QLatin1String(name)))
            return false;
    }
    m_language = language;
    m_userWords.clear();
    m_ignoredWords.clear();
    loadUserDictionary();
    loadIgnoredWords();
    applyEngineConfig();
    return true;
}

void SpellChecker::loadUserDictionary()
{
    for (const QString &word : readUserDictionaryWords())
        m_userWords.insert(word);
}

void SpellChecker::loadIgnoredWords()
{
    for (const QString &word : readIgnoredWords())
        m_ignoredWords.insert(word);
}

void SpellChecker::applyEngineConfig()
{
    if (!isLoaded())
        return;

    // Reconfiguring reloads every dictionary from disk and rebuilds the lookup
    // tables — a costly, tab-count-scaling operation that ran on every
    // Preferences OK even when nothing had changed. Skip it when the effective
    // configuration is identical to the last one applied.
    //
    // With a corpus active in override mode (sect.22) the corpus word sets
    // replace the global user.dic/ignored.dic sets entirely; otherwise all of
    // them union. "Corpus active" here means non-empty corpus sets — a plain
    // editor (no corpus, still the default override flag) must keep its global
    // user dictionary, so empty corpus sets behave exactly like merge mode.
    const bool corpusActive = !m_corpusWords.isEmpty() || !m_corpusIgnored.isEmpty();
    QStringList merged;
    if (m_corpusMerge || !corpusActive) {
        merged = m_userWords.values();
        merged << m_ignoredWords.values();
    }
    merged << m_corpusWords.values();
    merged << m_corpusIgnored.values();
    merged << importedWords();
    merged.removeDuplicates();
    merged.sort();

    QString key = m_language + QLatin1Char('|') + m_dialect
                  + QLatin1Char('|') + merged.join(QLatin1Char(','));
    if (key == m_configKey)
        return;
    m_configKey = key;

    if (m_dialect == QStringLiteral("British"))
        m_engine.setDialect(stoppard::Dialect::British);
    else if (m_dialect == QStringLiteral("Australian"))
        m_engine.setDialect(stoppard::Dialect::Australian);
    else if (m_dialect == QStringLiteral("Indian"))
        m_engine.setDialect(stoppard::Dialect::Indian);
    else if (m_dialect == QStringLiteral("Canadian"))
        m_engine.setDialect(stoppard::Dialect::Canadian);
    else if (m_dialect == QStringLiteral("New Zealand"))
        m_engine.setDialect(stoppard::Dialect::NewZealand);
    else
        m_engine.setDialect(stoppard::Dialect::American);

    m_engine.setLanguage(m_language == QLatin1String("en_GB")
                             ? stoppard::Language::British
                             : stoppard::Language::American);
    m_engine.setDictionaryPaths((bundledDictDir() + "/en-US.txt").toStdString(),
                                (bundledDictDir() + "/en-GB.txt").toStdString(),
                                (bundledDictDir() + "/maori-nz.txt").toStdString(),
                                (bundledDictDir() + "/canadian-en.txt").toStdString());

    std::vector<std::u16string> words;
    words.reserve(static_cast<size_t>(merged.size()));
    for (const QString &w : merged)
        words.push_back(w.toStdU16String());
    m_engine.setUserWords(std::move(words));
    ++m_configLoads;
}

void SpellChecker::setDialect(const QString &dialect)
{
    m_dialect = dialect;
    if (isLoaded())
        applyEngineConfig();
}

void SpellChecker::addToUserDictionary(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || m_userWords.contains(trimmed))
        return;

    m_userWords.insert(trimmed);
    writeUserDictionaryWords(m_userWords.values());
    applyEngineConfig();
}

void SpellChecker::removeFromUserDictionary(const QString &word)
{
    if (!m_userWords.remove(word))
        return;

    writeUserDictionaryWords(m_userWords.values());
    // The engine's user-word set is swapped atomically — no reload needed.
    applyEngineConfig();
}

QStringList SpellChecker::userWords() const
{
    QStringList words = m_userWords.values();
    words.sort();
    return words;
}

void SpellChecker::addToIgnored(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || m_ignoredWords.contains(trimmed))
        return;

    m_ignoredWords.insert(trimmed);
    writeIgnoredWords(m_ignoredWords.values());
    applyEngineConfig();
}

void SpellChecker::removeFromIgnored(const QString &word)
{
    if (!m_ignoredWords.remove(word))
        return;

    writeIgnoredWords(m_ignoredWords.values());
    // The engine's user-word set is swapped atomically — no reload needed.
    applyEngineConfig();
}

QStringList SpellChecker::ignoredWords() const
{
    QStringList words = m_ignoredWords.values();
    words.sort();
    return words;
}

void SpellChecker::setCorpusWords(const QStringList &words)
{
    m_corpusWords = QSet<QString>(words.begin(), words.end());
    applyEngineConfig();
}

void SpellChecker::setCorpusIgnored(const QStringList &words)
{
    m_corpusIgnored = QSet<QString>(words.begin(), words.end());
    applyEngineConfig();
}

void SpellChecker::setCorpusMerge(bool merge)
{
    if (m_corpusMerge == merge)
        return;
    m_corpusMerge = merge;
    applyEngineConfig();
}

void SpellChecker::addCorpusWord(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || m_corpusWords.contains(trimmed))
        return;
    m_corpusWords.insert(trimmed);
    applyEngineConfig();
}

void SpellChecker::removeCorpusWord(const QString &word)
{
    if (!m_corpusWords.remove(word))
        return;
    applyEngineConfig();
}

void SpellChecker::addCorpusIgnored(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || m_corpusIgnored.contains(trimmed))
        return;
    m_corpusIgnored.insert(trimmed);
    applyEngineConfig();
}

QStringList SpellChecker::corpusWords() const
{
    QStringList words = m_corpusWords.values();
    words.sort();
    return words;
}

QStringList SpellChecker::corpusIgnored() const
{
    QStringList words = m_corpusIgnored.values();
    words.sort();
    return words;
}

bool SpellChecker::checkWord(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || trimmed.size() == 1)
        return true;
    if (!isLoaded())
        return true;
    return !m_engine.isMisspelled(trimmed.toStdU16String());
}

QStringList SpellChecker::suggestions(const QString &word)
{
    if (!isLoaded())
        return {};
    QStringList result;
    for (const std::u16string &s : m_engine.spellSuggestions(word.trimmed().toStdU16String()))
        result << QString::fromStdU16String(s);
    return result;
}