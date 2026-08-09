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
#include <gtest/gtest.h>
#include "StaticHelpers.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

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

TEST(IsSafePreviewImageTest, AcceptsKnownRasterFormats) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    for (const QString &suffix : {"png", "jpg", "jpeg", "gif", "webp",
                                  "bmp", "svg", "avif", "ico", "tif", "tiff"}) {
        const QString path = dir.path() + "/image." + suffix;
        QFile f(path);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        EXPECT_TRUE(isSafePreviewImage(path)) << suffix.toStdString();
    }
}

TEST(IsSafePreviewImageTest, AcceptsUppercaseSuffix) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString path = dir.path() + "/image.PNG";
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    EXPECT_TRUE(isSafePreviewImage(path));
}

TEST(IsSafePreviewImageTest, RejectsMissingFile) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    EXPECT_FALSE(isSafePreviewImage(dir.path() + "/nope.png"));
}

TEST(IsSafePreviewImageTest, RejectsDirectory) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QDir().mkpath(dir.path() + "/folder.png");
    EXPECT_FALSE(isSafePreviewImage(dir.path() + "/folder.png"));
}

TEST(IsSafePreviewImageTest, RejectsNonImageExtension) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    for (const QString &suffix : {"txt", "md", "html", "pdf", "exe", ""}) {
        QString path = dir.path() + "/file" + (suffix.isEmpty() ? "" : "." + suffix);
        if (!suffix.isEmpty()) {
            QFile f(path);
            ASSERT_TRUE(f.open(QIODevice::WriteOnly));
            f.write("x");
            f.close();
        }
        EXPECT_FALSE(isSafePreviewImage(path)) << suffix.toStdString();
    }
}

TEST(EmojiCatalogTest, ParsesNonEmpty) {
    EXPECT_FALSE(emojiCatalog().isEmpty());
}

TEST(EmojiCatalogTest, ShortcodesUnique) {
    QSet<QString> seen;
    for (const EmojiEntry &e : emojiCatalog()) {
        EXPECT_FALSE(seen.contains(e.shortcode)) << "duplicate shortcode " << e.shortcode.toUtf8().constData();
        seen.insert(e.shortcode);
    }
}

TEST(EmojiCatalogTest, SortedAlphabetically) {
    const QList<EmojiEntry> catalog = emojiCatalog();
    for (int i = 1; i < catalog.size(); ++i)
        EXPECT_LT(catalog[i - 1].shortcode, catalog[i].shortcode);
}

TEST(EmojiCatalogTest, KnowShortcodesPresent) {
    QStringList codes;
    for (const EmojiEntry &e : emojiCatalog())
        codes << e.shortcode;
    EXPECT_TRUE(codes.contains("smile"));
    EXPECT_TRUE(codes.contains("heartpulse"));
    EXPECT_TRUE(codes.contains("woman-facepalming"));
}

TEST(EmojiCatalogTest, CodePointConsistentWithTwemojiPath) {
    for (const EmojiEntry &e : emojiCatalog()) {
        EXPECT_FALSE(e.codePoint.isEmpty());
        const QString svg = QString(":/twemoji/svg/%1.svg").arg(e.codePoint);
        if (QFile::exists(svg))
            EXPECT_EQ(emojiTwemojiPath(e.unicode), svg);
        else
            EXPECT_TRUE(emojiTwemojiPath(e.unicode).isEmpty())
                << e.shortcode.toUtf8().constData() << " has an SVG but no codePoint SVG";
    }
}

TEST(EmojiTwemojiPathTest, SimpleEmojiResolves) {
    // 😄 (U+1F604) has a plain SVG, no fe0f stripping.
    EXPECT_EQ(emojiTwemojiPath(QString::fromUtf8("\xF0\x9F\x98\x84")),
        ":/twemoji/svg/1f604.svg");
}

TEST(EmojiTwemojiPathTest, ZWJSequenceFallsBackToFullCodepoints) {
    // 🤦&zwj;♀️ keeps its fe0f in the twemoji filename, so the stripped
    // candidate must not win.
    EXPECT_EQ(emojiTwemojiPath(QString::fromUtf8("\xF0\x9F\xA4\xA6\xE2\x80\x8D\xE2\x99\x80\xEF\xB8\x8F")),
        ":/twemoji/svg/1f926-200d-2640-fe0f.svg");
}

TEST(EmojiTwemojiPathTest, UnknownReturnsEmpty) {
    EXPECT_TRUE(emojiTwemojiPath("not-an-emoji").isEmpty());
}

TEST(FuzzyMatchScoreTest, EmptyFragmentMatchesEverything) {
    EXPECT_TRUE(fuzzyMatchScore("abc", "").matched);
    EXPECT_EQ(fuzzyMatchScore("abc", "").gaps, 0);
    EXPECT_EQ(fuzzyMatchScore("abc", "").firstPos, 0);
}

TEST(FuzzyMatchScoreTest, SubstringMatchesWithZeroGaps) {
    FuzzyScore s = fuzzyMatchScore("scriba.svg", "scrsvg");
    ASSERT_TRUE(s.matched);
    EXPECT_EQ(s.gaps, 4);       // skips i, b, a, . between r and s
    EXPECT_EQ(s.firstPos, 0);
}

TEST(FuzzyMatchScoreTest, PrefixScoreIsZeroZero) {
    FuzzyScore s = fuzzyMatchScore("happy", "hap");
    ASSERT_TRUE(s.matched);
    EXPECT_EQ(s.gaps, 0);
    EXPECT_EQ(s.firstPos, 0);
}

TEST(FuzzyMatchScoreTest, ScatteredMatchCountsGaps) {
    // "prt" in "pirate": p(0), r(2), t(4) -> gaps = 1 + 1 = 2.
    FuzzyScore s = fuzzyMatchScore("pirate", "prt");
    ASSERT_TRUE(s.matched);
    EXPECT_EQ(s.gaps, 2);
    EXPECT_EQ(s.firstPos, 0);
}

TEST(FuzzyMatchScoreTest, CaseInsensitive) {
    FuzzyScore s = fuzzyMatchScore("HAPPY", "hap");
    ASSERT_TRUE(s.matched);
    EXPECT_EQ(s.gaps, 0);
}

TEST(FuzzyMatchScoreTest, NoSequentialMatch) {
    EXPECT_FALSE(fuzzyMatchScore("abc", "cba").matched);
    EXPECT_FALSE(fuzzyMatchScore("smiley", "smilz").matched);
}



