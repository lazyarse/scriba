#include <gtest/gtest.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QString>

#include "CssConfig.h"
#include "CssLoader.h"
#include "CssUtils.h"

namespace {

QString configDir()
{
    return CssUtils::scribaConfigDir();
}

void resetConfigDir()
{
    QDir dir(configDir());
    dir.removeRecursively();
    dir.mkpath(".");
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

QString bundledPreviewCss()
{
    return readFile(":/preview-base.css");
}

QString bundledPrintCss()
{
    return readFile(":/print-base.css");
}

QString previewHash()
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bundledPreviewCss().toUtf8(), QCryptographicHash::Sha256).toHex());
}

} // namespace

class CssLoaderTest : public testing::Test {
protected:
    void SetUp() override
    {
        resetConfigDir();
    }
};

TEST_F(CssLoaderTest, NoConfigCopyUsesBundledCss)
{
    CssConfig config;
    CssLoader loader(&config);
    EXPECT_EQ(loader.previewBaseCss(), bundledPreviewCss());
    EXPECT_EQ(loader.printBaseCss(), bundledPrintCss());
    EXPECT_TRUE(loader.staleBaseCssFiles().isEmpty());
}

TEST_F(CssLoaderTest, MatchingMarkerCopyIsHonoured)
{
    QFile f(configDir() + "/preview-base.css");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(("/* scriba-base-css-version: " + previewHash() + " */\nbody { color: red; }\n").toUtf8());
    f.close();

    CssConfig config;
    CssLoader loader(&config);
    EXPECT_EQ(loader.previewBaseCss(), "body { color: red; }\n");
    EXPECT_TRUE(loader.staleBaseCssFiles().isEmpty());
}

TEST_F(CssLoaderTest, MissingMarkerCopyIsSuperseded)
{
    QFile f(configDir() + "/preview-base.css");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("body { color: red; }\n");
    f.close();

    CssConfig config;
    CssLoader loader(&config);
    EXPECT_EQ(loader.previewBaseCss(), bundledPreviewCss());
    ASSERT_EQ(loader.staleBaseCssFiles().size(), 1);
    EXPECT_EQ(loader.staleBaseCssFiles().first(), configDir() + "/preview-base.css");
    EXPECT_FALSE(QFile::exists(configDir() + "/preview-base.css"));
    EXPECT_TRUE(QFile::exists(configDir() + "/preview-base.css.bak"));
    EXPECT_EQ(readFile(configDir() + "/preview-base.css.bak"), "body { color: red; }\n");
}

TEST_F(CssLoaderTest, OutdatedMarkerCopyIsSuperseded)
{
    QFile f(configDir() + "/preview-base.css");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("/* scriba-base-css-version: 0000000000000000000000000000000000000000000000000000000000000000 */\nbody {}\n");
    f.close();

    CssConfig config;
    CssLoader loader(&config);
    EXPECT_EQ(loader.previewBaseCss(), bundledPreviewCss());
    ASSERT_EQ(loader.staleBaseCssFiles().size(), 1);
    EXPECT_TRUE(QFile::exists(configDir() + "/preview-base.css.bak"));
    EXPECT_EQ(readFile(configDir() + "/preview-base.css.bak"), "/* scriba-base-css-version: 0000000000000000000000000000000000000000000000000000000000000000 */\nbody {}\n");
}

TEST_F(CssLoaderTest, SetterWritesMarkerAndRoundTrips)
{
    CssConfig config;
    CssLoader loader(&config);
    loader.setPreviewBaseCss("body { font-size: 13pt; }\n");

    QString saved = readFile(configDir() + "/preview-base.css");
    EXPECT_TRUE(saved.startsWith("/* scriba-base-css-version: "));
    EXPECT_TRUE(saved.contains("/* scriba-base-css-version: " + previewHash() + " */"));
    EXPECT_TRUE(saved.contains("body { font-size: 13pt; }"));

    CssLoader loader2(&config);
    EXPECT_EQ(loader2.previewBaseCss(), "body { font-size: 13pt; }\n");
    EXPECT_TRUE(loader2.staleBaseCssFiles().isEmpty());
}

TEST_F(CssLoaderTest, PrintBaseCssGetsSameTreatment)
{
    QFile f(configDir() + "/print-base.css");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("/* scriba-base-css-version: 0000000000000000000000000000000000000000000000000000000000000000 */\n@page {}\n");
    f.close();

    CssConfig config;
    CssLoader loader(&config);
    EXPECT_EQ(loader.printBaseCss(), bundledPrintCss());
    EXPECT_EQ(loader.staleBaseCssFiles().size(), 1);
    EXPECT_TRUE(QFile::exists(configDir() + "/print-base.css.bak"));

    loader.clearStaleBaseCssFlags();
    EXPECT_TRUE(loader.staleBaseCssFiles().isEmpty());
}
