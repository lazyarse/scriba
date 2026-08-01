#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>

#include "TestConfig.h"

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    QApplication app(argc, argv);
    setupTestConfig();
    return RUN_ALL_TESTS();
}
