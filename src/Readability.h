#pragma once

#include <QString>
#include <QStringList>

int countSentences(const QString &text);
int estimateSyllables(const QString &word);
int countCharactersWithoutSpaces(const QString &text);
int countCharactersWithSpaces(const QString &text);
int countParagraphs(const QString &text);
int countComplexWords(const QStringList &words);
double lexicalDensity(const QStringList &words);
double fleschReadingEase(int words, int sentences, int syllables);
double fleschKincaidGrade(int words, int sentences, int syllables);
double colemanLiauGrade(int words, int sentences, int characters);
double gunningFogGrade(int words, int sentences, int complexWords);
double smogGrade(int sentences, int polysyllables);
double ariGrade(int words, int sentences, int characters);
