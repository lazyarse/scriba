#include "SpellChecker.h"
#include "CssUtils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    return CssUtils::scribaConfigDir() + "/dictionaries";
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

bool SpellChecker::isBundledLanguage(const QString &language)
{
    for (const char *bundled : BundledLangs) {
        if (language == QLatin1String(bundled))
            return true;
    }
    return false;
}

namespace {

// Restrict dictionary base names to the characters used by standard hunspell
// language codes (e.g. "de_DE", "ca_ES-valencia") and refuse the reserved
// "user"/"bundled" names so a dictionary can never collide with the user word
// list or the bundled extraction directory.
bool isSafeDictionaryBase(const QString &base)
{
    if (base.isEmpty() || base == "." || base == "..")
        return false;
    if (base == QLatin1String("user") || base == QLatin1String("bundled"))
        return false;
    for (QChar c : base) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('-'))
            return false;
    }
    return true;
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

// Hunspell tolerates corrupt files silently (it builds an empty table), so
// validate structurally: a .dic must start with a parseable word count and a
// .aff must be non-empty.
bool looksLikeDictionary(const QString &affPath, const QString &dicPath)
{
    QFile aff(affPath);
    if (!aff.open(QIODevice::ReadOnly | QIODevice::Text) || aff.size() == 0)
        return false;

    QFile dic(dicPath);
    if (!dic.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    bool ok = false;
    const int count = dic.readLine().trimmed().toInt(&ok);
    return ok && count >= 0;
}

} // namespace

QString SpellChecker::installDictionary(const QString &affOrDicPath)
{
    QFileInfo info(affOrDicPath);
    if (!info.exists() || !info.isFile())
        return {};

    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().toLower();
    if (!isSafeDictionaryBase(base) || isBundledLanguage(base))
        return {};

    QString affPath, dicPath;
    if (suffix == "aff") {
        affPath = affOrDicPath;
        dicPath = info.dir().filePath(base + ".dic");
    } else if (suffix == "dic") {
        dicPath = affOrDicPath;
        affPath = info.dir().filePath(base + ".aff");
    } else {
        return {};
    }

    QDir().mkpath(configDictDir());
    const QString dstAff = configDictDir() + "/" + base + ".aff";
    const QString dstDic = configDictDir() + "/" + base + ".dic";

    if (QFileInfo::exists(dstAff) && QFileInfo::exists(dstDic))
        return base;

    if (!copyFileTo(affPath, dstAff) || !copyFileTo(dicPath, dstDic)) {
        QFile::remove(dstAff);
        QFile::remove(dstDic);
        return {};
    }
    if (!looksLikeDictionary(dstAff, dstDic)) {
        QFile::remove(dstAff);
        QFile::remove(dstDic);
        return {};
    }
    return base;
}

bool SpellChecker::removeDictionary(const QString &language)
{
    if (isBundledLanguage(language) || !isSafeDictionaryBase(language))
        return false;

    QDir dir(configDictDir());
    bool removed = QFile::remove(dir.filePath(language + ".aff"));
    removed = QFile::remove(dir.filePath(language + ".dic")) || removed;
    return removed;
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

bool SpellChecker::checkWord(const QString &word)
{
    QString trimmed = word.trimmed();
    if (trimmed.isEmpty() || trimmed.size() == 1)
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
