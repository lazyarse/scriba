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
