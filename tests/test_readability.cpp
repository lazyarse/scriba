#include <gtest/gtest.h>
#include "StaticHelpers.h"

TEST(CountSentences, EmptyReturnsOne) {
    EXPECT_EQ(countSentences(""), 1);
}

TEST(CountSentences, SingleSentence) {
    EXPECT_EQ(countSentences("Hello world"), 1);
}

TEST(CountSentences, TwoSentences) {
    EXPECT_EQ(countSentences("Hello world. How are you?"), 2);
}

TEST(CountSentences, ThreeSentences) {
    EXPECT_EQ(countSentences("One! Two? Three."), 3);
}

TEST(CountSentences, MultiplePeriods) {
    EXPECT_EQ(countSentences("Dr. Smith went to Washington."), 1);
}

TEST(EstimateSyllables, EmptyReturnsZero) {
    EXPECT_EQ(estimateSyllables(""), 0);
}

TEST(EstimateSyllables, ShortWordReturnsOne) {
    EXPECT_EQ(estimateSyllables("cat"), 1);
}

TEST(EstimateSyllables, TwoLetterWord) {
    EXPECT_EQ(estimateSyllables("a"), 1);
}

TEST(EstimateSyllables, Banana) {
    EXPECT_EQ(estimateSyllables("banana"), 3);
}

TEST(EstimateSyllables, Make) {
    EXPECT_EQ(estimateSyllables("make"), 1);
}

TEST(EstimateSyllables, Apple) {
    EXPECT_EQ(estimateSyllables("apple"), 2);
}

TEST(EstimateSyllables, Beautiful) {
    EXPECT_EQ(estimateSyllables("beautiful"), 3);
}

TEST(EstimateSyllables, CaseInsensitive) {
    EXPECT_EQ(estimateSyllables("BANANA"), 3);
}

TEST(FleschKincaidGrade, ZeroWords) {
    EXPECT_DOUBLE_EQ(fleschKincaidGrade(0, 5, 10), 0.0);
}

TEST(FleschKincaidGrade, ZeroSentences) {
    EXPECT_DOUBLE_EQ(fleschKincaidGrade(100, 0, 150), 0.0);
}

TEST(FleschKincaidGrade, SimpleText) {
    // "The cat sat on the mat." = 6 words, 1 sentence, 6 syllables
    // FK = 0.39*(6/1) + 11.8*(6/6) - 15.59 = 2.34 + 11.8 - 15.59 = -1.45
    double grade = fleschKincaidGrade(6, 1, 6);
    EXPECT_NEAR(grade, -1.45, 0.1);
}

TEST(FleschKincaidGrade, ComplexText) {
    // 20 words, 2 sentences, 30 syllables
    // FK = 0.39*(20/2) + 11.8*(30/20) - 15.59 = 3.9 + 17.7 - 15.59 = 6.01
    double grade = fleschKincaidGrade(20, 2, 30);
    EXPECT_NEAR(grade, 6.01, 0.1);
}

TEST(CountCharactersWithoutSpaces, Empty) {
    EXPECT_EQ(countCharactersWithoutSpaces(""), 0);
}

TEST(CountCharactersWithoutSpaces, Basic) {
    EXPECT_EQ(countCharactersWithoutSpaces("hello world"), 10);
}

TEST(CountCharactersWithoutSpaces, WithPunctuation) {
    EXPECT_EQ(countCharactersWithoutSpaces("Hi, there!"), 9);
}

TEST(CountComplexWords, EmptyList) {
    QStringList empty;
    EXPECT_EQ(countComplexWords(empty), 0);
}

TEST(CountComplexWords, SomeComplex) {
    // "cat" = 1 syl, "banana" = 3 syl, "beautiful" = 3 syl, "make" = 1 syl
    QStringList words = {"cat", "banana", "beautiful", "make"};
    EXPECT_EQ(countComplexWords(words), 2);
}

TEST(ColemanLiauGrade, ZeroWords) {
    EXPECT_DOUBLE_EQ(colemanLiauGrade(0, 5, 100), 0.0);
}

TEST(ColemanLiauGrade, SimpleText) {
    // 6 words, 1 sentence, 25 chars (no spaces): "Thecatonthemat"
    // L = 100*25/6 ≈ 416.67, S = 100*1/6 ≈ 16.67
    // CL = 0.0588*416.67 - 0.296*16.67 - 15.8 = 24.5 - 4.93 - 15.8 = 3.77
    double grade = colemanLiauGrade(6, 1, 25);
    EXPECT_NEAR(grade, 3.77, 0.2);
}

TEST(ColemanLiauGrade, ComplexText) {
    double grade = colemanLiauGrade(20, 2, 90);
    EXPECT_NEAR(grade, 7.54, 0.5);
}

TEST(GunningFogGrade, ZeroWords) {
    EXPECT_DOUBLE_EQ(gunningFogGrade(0, 1, 0), 0.0);
}

TEST(GunningFogGrade, SimpleText) {
    // 6 words, 1 sentence, 0 complex words
    // Fog = 0.4*(6/1 + 100*0/6) = 0.4*6 = 2.4
    double grade = gunningFogGrade(6, 1, 0);
    EXPECT_NEAR(grade, 2.4, 0.1);
}

TEST(GunningFogGrade, WithComplexWords) {
    // 10 words, 1 sentence, 3 complex
    // Fog = 0.4*(10/1 + 100*3/10) = 0.4*(10 + 30) = 16.0
    double grade = gunningFogGrade(10, 1, 3);
    EXPECT_NEAR(grade, 16.0, 0.1);
}

TEST(SmogGrade, ZeroSentences) {
    EXPECT_DOUBLE_EQ(smogGrade(0, 10), 0.0);
}

TEST(SmogGrade, SimpleText) {
    // 10 sentences, 5 polysyllables
    // SMOG = 1.0430*sqrt(5*30/10) + 3.1291 = 1.0430*sqrt(15) + 3.1291 ≈ 7.17
    double grade = smogGrade(10, 5);
    EXPECT_NEAR(grade, 7.17, 0.2);
}

TEST(AriGrade, ZeroWords) {
    EXPECT_DOUBLE_EQ(ariGrade(0, 1, 0), 0.0);
}

TEST(AriGrade, SimpleText) {
    // 6 words, 1 sentence, 25 chars
    // ARI = 4.71*(25/6) + 0.5*(6/1) - 21.43 = 19.625 + 3.0 - 21.43 = 1.195
    double grade = ariGrade(6, 1, 25);
    EXPECT_NEAR(grade, 1.2, 0.2);
}
