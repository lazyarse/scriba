#include "SpellChecker.h"
#include "Preferences.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <hunspell.hxx>

namespace {

constexpr const char *BundledLangs[] = {"en_US", "en_GB"};

QString bundledDictDir()
{
    return SpellChecker::configDictDir() + "/bundled";
}

bool copyBundledIfNeeded(const QString &name)
{
    QDir().mkpath(bundledDictDir());
    bool ok = true;
    for (const char *ext : {".aff", ".dic"}) {
        QString target = bundledDictDir() + "/" + name + ext;
        QString source = ":/dictionaries/" + name + ext;
        QFile src(source);
        if (!src.exists())
            continue;
        if (QFileInfo::exists(target)
            && QFileInfo(target).size() == src.size())
            continue;
        if (!src.open(QIODevice::ReadOnly)) {
            ok = false;
            continue;
        }
        QFile dst(target);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            ok = false;
            continue;
        }
        dst.write(src.readAll());
    }
    return ok;
}

} // namespace

SpellChecker::SpellChecker() = default;

SpellChecker::~SpellChecker() = default;

QString SpellChecker::configDictDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + "/scriba/dictionaries";
}

QString SpellChecker::userDictPath() const
{
    return configDictDir() + "/user.dic";
}

QStringList SpellChecker::readUserDictionaryWords()
{
    QFile file(configDictDir() + "/user.dic");
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

void SpellChecker::writeUserDictionaryWords(const QStringList &words)
{
    QStringList sorted = words;
    sorted.sort();
    sorted.removeDuplicates();

    QStringList lines;
    lines << QString::number(sorted.size());
    for (const QString &w : sorted)
        lines << w;
    QDir().mkpath(configDictDir());
    QFile file(configDictDir() + "/user.dic");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        file.write(lines.join('\n').toUtf8());
        file.write("\n");
    }
}

QStringList SpellChecker::readIgnoreList()
{
    QSettings settings;
    return settings.value(Preferences::IgnoreList).toStringList();
}

void SpellChecker::writeIgnoreList(const QStringList &words)
{
    QStringList sorted;
    for (const QString &w : words) {
        QString lower = w.trimmed().toLower();
        if (!lower.isEmpty() && !sorted.contains(lower))
            sorted << lower;
    }
    sorted.sort();
    QSettings settings;
    settings.setValue(Preferences::IgnoreList, sorted);
    settings.sync();
}

bool SpellChecker::findDictionaryFiles(const QString &language, QString &aff, QString &dic) const
{
    for (const char *bundled : BundledLangs) {
        if (language == QLatin1String(bundled)) {
            aff = bundledDictDir() + "/" + language + ".aff";
            dic = bundledDictDir() + "/" + language + ".dic";
            return QFileInfo::exists(aff) && QFileInfo::exists(dic);
        }
    }
    aff = configDictDir() + "/" + language + ".aff";
    dic = configDictDir() + "/" + language + ".dic";
    return QFileInfo::exists(aff) && QFileInfo::exists(dic);
}

QStringList SpellChecker::availableLanguages()
{
    QStringList langs;
    for (const char *bundled : BundledLangs) {
        if (copyBundledIfNeeded(bundled))
            langs << QLatin1String(bundled);
    }
    QDir dir(configDictDir());
    const QStringList affFiles = dir.entryList({"*.aff"}, QDir::Files);
    for (const QString &aff : affFiles) {
        QString base = QFileInfo(aff).completeBaseName();
        if (base.isEmpty() || base == "user")
            continue;
        if (dir.exists(base + ".dic") && !langs.contains(base))
            langs << base;
    }
    langs.sort();
    return langs;
}

bool SpellChecker::loadLanguage(const QString &language)
{
    copyBundledIfNeeded("en_US");
    copyBundledIfNeeded("en_GB");

    QString aff, dic;
    if (!findDictionaryFiles(language, aff, dic))
        return false;

    m_hunspell = std::make_unique<Hunspell>(aff.toUtf8().constData(), dic.toUtf8().constData());
    if (!m_hunspell)
        return false;

    m_language = language;

    QSettings settings;
    const QStringList ignored = settings.value(Preferences::IgnoreList).toStringList();
    m_ignores = QSet<QString>(ignored.cbegin(), ignored.cend());

    m_userWords.clear();
    loadUserDictionary();

    return true;
}

void SpellChecker::loadUserDictionary()
{
    QFile file(userDictPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return;

    bool ok = false;
    int count = lines.first().trimmed().toInt(&ok);
    if (!ok || count <= 0)
        return;

    for (int i = 1; i < lines.size() && i <= count; ++i) {
        QString word = lines.at(i).trimmed();
        if (word.isEmpty())
            continue;
        m_userWords.insert(word);
        m_hunspell->add(word.toStdString());
    }
}

void SpellChecker::addToUserDictionary(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || m_userWords.contains(trimmed))
        return;

    if (m_hunspell)
        m_hunspell->add(trimmed.toStdString());

    m_userWords.insert(trimmed);
    writeUserDictionaryWords(m_userWords.values());
}

void SpellChecker::removeFromUserDictionary(const QString &word)
{
    if (!m_userWords.remove(word))
        return;

    writeUserDictionaryWords(m_userWords.values());

    // hunspell has no runtime "remove" — rebuild the active instance so the
    // word is flagged again immediately (also reloads ignores + user words).
    if (!m_language.isEmpty())
        loadLanguage(m_language);
}

QStringList SpellChecker::userWords() const
{
    QStringList words = m_userWords.values();
    words.sort();
    return words;
}

void SpellChecker::ignoreWord(const QString &word)
{
    m_ignores.insert(word.trimmed().toLower());
}

void SpellChecker::ignoreAll(const QString &word)
{
    ignoreWord(word);
    writeIgnoreList(m_ignores.values());
}

QStringList SpellChecker::ignoredWords() const
{
    QStringList words = m_ignores.values();
    words.sort();
    return words;
}

bool SpellChecker::isIgnored(const QString &word) const
{
    return m_ignores.contains(word.trimmed().toLower());
}

bool SpellChecker::checkWord(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || trimmed.size() == 1)
        return true;
    if (isIgnored(trimmed))
        return true;
    if (!m_hunspell)
        return true;
    return m_hunspell->spell(trimmed.toStdString());
}

QStringList SpellChecker::suggestions(const QString &word)
{
    if (!m_hunspell)
        return {};
    QStringList result;
    char **slst = nullptr;
    for (const std::string &s : m_hunspell->suggest(word.trimmed().toStdString()))
        result << QString::fromStdString(s);
    return result;
}
