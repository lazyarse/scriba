#include "CsvReader.h"
#include <QFile>
#include <QTextStream>

CsvData CsvReader::readFromFile(const QString &path, bool firstRowIsHeaders)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&f);
    QString text = in.readAll();
    return parse(text, firstRowIsHeaders);
}

CsvData CsvReader::readFromString(const QString &csv, bool firstRowIsHeaders)
{
    return parse(csv, firstRowIsHeaders);
}

CsvData CsvReader::parse(const QString &text, bool firstRowIsHeaders)
{
    CsvData data;
    QStringList lines = text.split('\n');
    if (lines.isEmpty())
        return data;

    int start = 0;
    if (firstRowIsHeaders && !lines[0].trimmed().isEmpty()) {
        data.headers = splitLine(lines[0].trimmed());
        start = 1;
    }

    for (int i = start; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty())
            continue;
        QStringList fields = splitLine(line);
        if (data.headers.isEmpty() && fields.size() > 0) {
            for (int c = 0; c < fields.size(); ++c)
                data.headers << QString("Column %1").arg(c + 1);
        }
        data.rows.append(fields);
    }

    if (data.headers.isEmpty() && !data.rows.isEmpty()) {
        int maxCols = 0;
        for (const auto &row : data.rows)
            maxCols = qMax(maxCols, row.size());
        for (int c = 0; c < maxCols; ++c)
            data.headers << QString("Column %1").arg(c + 1);
    }

    return data;
}

QStringList CsvReader::splitLine(const QString &line)
{
    QStringList result;
    QString field;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        QChar c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                result.append(field.trimmed());
                field.clear();
            } else {
                field += c;
            }
        }
    }
    result.append(field.trimmed());
    return result;
}
