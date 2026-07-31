#include <gtest/gtest.h>
#include "StaticHelpers.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

TEST(EscapeJsStringTest, EmptyString) {
    EXPECT_EQ(escapeJsString(""), "");
}

TEST(EscapeJsStringTest, NoSpecialChars) {
    EXPECT_EQ(escapeJsString("hello world"), "hello world");
}

TEST(EscapeJsStringTest, Backslash) {
    EXPECT_EQ(escapeJsString("a\\b"), "a\\\\b");
}

TEST(EscapeJsStringTest, SingleQuote) {
    EXPECT_EQ(escapeJsString("it's"), "it\\'s");
}

TEST(EscapeJsStringTest, Newline) {
    EXPECT_EQ(escapeJsString("a\nb"), "a\\nb");
}

TEST(EscapeJsStringTest, MultipleSpecialChars) {
    EXPECT_EQ(escapeJsString("a\\b'c\nd"), "a\\\\b\\'c\\nd");
}

TEST(DuplicateCssFileTest, CopiesBundledQrcTheme) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString newPath = duplicateCssFile(":/themes/github-light.css", dir.path());
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_EQ(QFileInfo(newPath).fileName(), "github-light-copy.css");
    EXPECT_TRUE(QFileInfo::exists(newPath));

    QFile f(newPath);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(QString::fromUtf8(f.readAll()), readResourceFile(":/themes/github-light.css"));
}

TEST(DuplicateCssFileTest, CopiesDiskFile) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QFile src(dir.path() + "/original.css");
    ASSERT_TRUE(src.open(QIODevice::WriteOnly | QIODevice::Text));
    src.write("body { color: red; }\n");
    src.close();

    QString newPath = duplicateCssFile(src.fileName(), dir.path());
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_EQ(QFileInfo(newPath).fileName(), "original-copy.css");

    QFile f(newPath);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(QString::fromUtf8(f.readAll()), "body { color: red; }\n");
}

TEST(DuplicateCssFileTest, HandlesNameCollisions) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString first = duplicateCssFile(":/themes/nord.css", dir.path());
    ASSERT_FALSE(first.isEmpty());
    EXPECT_EQ(QFileInfo(first).fileName(), "nord-copy.css");
    EXPECT_TRUE(duplicateCssFile(":/themes/nord.css", dir.path()).isEmpty())
        << "duplicate of an existing default name should fail";
}

TEST(DuplicateCssFileTest, UsesProvidedBaseName) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString newPath = duplicateCssFile(":/themes/nord.css", dir.path(), "my-theme");
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_EQ(QFileInfo(newPath).fileName(), "my-theme.css");
}

TEST(DuplicateCssFileTest, AppendsCssExtensionOnce) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString newPath = duplicateCssFile(":/themes/nord.css", dir.path(), "my-theme.css");
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_EQ(QFileInfo(newPath).fileName(), "my-theme.css");
}

TEST(DuplicateCssFileTest, SanitizesIllegalNameChars) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString newPath = duplicateCssFile(":/themes/nord.css", dir.path(), "my/theme:test");
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_EQ(QFileInfo(newPath).fileName(), "my-theme-test.css");
}

TEST(DuplicateCssFileTest, ProvidedNameCollisionReturnsEmpty) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    ASSERT_FALSE(duplicateCssFile(":/themes/nord.css", dir.path(), "taken").isEmpty());
    EXPECT_TRUE(duplicateCssFile(":/themes/nord.css", dir.path(), "taken").isEmpty());
}

TEST(DuplicateCssFileTest, CreatesMissingDestDir) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QString newPath = duplicateCssFile(":/themes/dracula.css",
        dir.path() + "/nested/themes");
    ASSERT_FALSE(newPath.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(newPath));
}

TEST(DuplicateCssFileTest, MissingSourceReturnsEmpty) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    EXPECT_TRUE(duplicateCssFile(dir.path() + "/does-not-exist.css", dir.path()).isEmpty());
}


