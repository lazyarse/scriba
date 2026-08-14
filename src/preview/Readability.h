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
