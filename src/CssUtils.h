#ifndef CSSUTILS_H
#define CSSUTILS_H

#include <QString>
#include <QColor>

namespace CssUtils {
    QString deriveChromeCss(const QString &themeCss);
    QColor chromeTextColor(const QString &themeCss);
}

#endif
