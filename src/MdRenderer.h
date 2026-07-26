#pragma once

#include <QString>
#include <md4c.h>

class MdRenderer
{
public:
    MdRenderer();

    QString render(const char *input, MD_SIZE size, unsigned parserFlags);

private:
    struct ImageState {
        bool inside = false;
        QString alt;
        QString src;
        QString title;
    };

    static int enterBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int leaveBlock(MD_BLOCKTYPE type, void *detail, void *userdata);
    static int enterSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int leaveSpan(MD_SPANTYPE type, void *detail, void *userdata);
    static int text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata);

    void writeHtml(const char *data, MD_SIZE size);
    void writeHtml(const QString &str);
    static QString escapeHtml(const QString &str);
    static QString escapeAttr(const QString &str);
    static QString alignmentStyle(MD_ALIGN align);
    static void parseDimensions(const QString &src, QString &cleanSrc, int &width, int &height);

    void enterCodeBlock(void *detail);
    void enterListItem(void *detail);
    void enterAdmonition(void *detail);
    void enterAlignedCell(void *detail, const char *tag);

    QString m_output;
    int m_currentLine = 1;
    ImageState m_img;
};

