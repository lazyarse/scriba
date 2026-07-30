#include "Readability.h"
#include <QSet>
#include <QTextBoundaryFinder>
#include <cmath>

int countSentences(const QString &text)
{
    if (text.isEmpty()) return 1;
    int count = 0;
    QTextBoundaryFinder finder(QTextBoundaryFinder::Sentence, text);
    while (finder.toNextBoundary() > 0)
        ++count;
    return qMax(1, count);
}

int estimateSyllables(const QString &word)
{
    if (word.isEmpty()) return 0;
    static const QSet<QChar> vowels = {'a','e','i','o','u','y'};
    QString lower = word.toLower();
    int count = 0;
    bool inGroup = false;
    for (int i = 0; i < lower.size(); ++i) {
        bool isVowel = vowels.contains(lower[i]);
        if (isVowel && !inGroup) {
            ++count;
            inGroup = true;
        } else if (!isVowel) {
            inGroup = false;
        }
    }
    if (lower.endsWith('e') && !lower.endsWith("le") && count > 1)
        --count;
    return qMax(1, count);
}

int countCharactersWithoutSpaces(const QString &text)
{
    int count = 0;
    for (const QChar &c : text) {
        if (!c.isSpace())
            ++count;
    }
    return count;
}

int countCharactersWithSpaces(const QString &text)
{
    return text.length();
}

int countParagraphs(const QString &text)
{
    if (text.isEmpty()) return 0;
    int count = 1;
    for (int i = 0; i < text.size() - 1; ++i) {
        if (text[i] == '\n' && text[i + 1] == '\n')
            ++count;
    }
    return count;
}

int countComplexWords(const QStringList &words)
{
    int count = 0;
    for (const QString &w : words) {
        if (estimateSyllables(w) >= 3)
            ++count;
    }
    return count;
}

double lexicalDensity(const QStringList &words)
{
    if (words.isEmpty()) return 0.0;
    QSet<QString> unique(words.begin(), words.end());
    return 100.0 * unique.size() / words.size();
}

double fleschReadingEase(int words, int sentences, int syllables)
{
    if (words == 0 || sentences == 0) return 0.0;
    return 206.835 - 1.015 * (static_cast<double>(words) / sentences)
         - 84.6 * (static_cast<double>(syllables) / words);
}

double fleschKincaidGrade(int words, int sentences, int syllables)
{
    if (words == 0 || sentences == 0) return 0.0;
    return 0.39 * (static_cast<double>(words) / sentences)
         + 11.8 * (static_cast<double>(syllables) / words)
         - 15.59;
}

double colemanLiauGrade(int words, int sentences, int characters)
{
    if (words == 0 || sentences == 0) return 0.0;
    double L = 100.0 * characters / words;
    double S = 100.0 * sentences / words;
    return 0.0588 * L - 0.296 * S - 15.8;
}

double gunningFogGrade(int words, int sentences, int complexWords)
{
    if (words == 0 || sentences == 0) return 0.0;
    return 0.4 * (static_cast<double>(words) / sentences
                  + 100.0 * complexWords / words);
}

double smogGrade(int sentences, int polysyllables)
{
    if (sentences == 0) return 0.0;
    return 1.0430 * std::sqrt(polysyllables * 30.0 / sentences) + 3.1291;
}

double ariGrade(int words, int sentences, int characters)
{
    if (words == 0 || sentences == 0) return 0.0;
    return 4.71 * (static_cast<double>(characters) / words)
         + 0.5 * (static_cast<double>(words) / sentences)
         - 21.43;
}
