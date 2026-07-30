#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct CsvData {
    QStringList headers;
    QVector<QStringList> rows;
};

class CsvReader
{
public:
    static CsvData readFromFile(const QString &path, bool firstRowIsHeaders = true);
    static CsvData readFromString(const QString &csv, bool firstRowIsHeaders = true);

private:
    static CsvData parse(const QString &text, bool firstRowIsHeaders);
    static QStringList splitLine(const QString &line);
};
