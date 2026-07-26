#pragma once

#include <QString>
#include <QStringList>

class CssConfig
{
public:
    CssConfig();

    static QStringList bundledThemes();

    QStringList stylesheets() const;
    void setStylesheets(const QStringList &paths);

    QString activeStylesheet() const;
    void setActiveStylesheet(const QString &path);

private:
    QStringList m_stylesheets;
    QString m_activeStylesheet;
};

