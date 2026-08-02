cmake_minimum_required(VERSION 3.21)
project(stoppard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(stoppard STATIC
    src/tokenizer.cpp
    src/lexicon.cpp
    src/morphology.cpp
    src/tagger.cpp
    src/chunker.cpp
    src/rules.cpp
    src/rules_modal.cpp
    src/rules_aux.cpp
    src/rules_agreement.cpp
    src/rules_to_infinitive.cpp
    src/rules_determiner.cpp
    src/rules_pronoun_case.cpp
    src/rules_double_modal.cpp
    src/rules_double_negative.cpp
    src/rules_regionalisms.cpp
    src/engine.cpp
)
target_include_directories(stoppard PUBLIC include)
target_compile_options(stoppard PRIVATE -Wall -Wextra -Wpedantic)

option(STOPPARD_BUILD_TESTS "Build stoppard tests" ON)

if(STOPPARD_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
    enable_testing()

    add_executable(test_stoppard
        tests/test_tokenizer.cpp
        tests/test_lexicon.cpp
        tests/test_morphology.cpp
        tests/test_smoke.cpp
        tests/test_tagger.cpp
        tests/test_chunker.cpp
        tests/test_rules_framework.cpp
        tests/test_rules_modal.cpp
        tests/test_rules_aux.cpp
        tests/test_rules_agreement.cpp
        tests/test_rules_to_infinitive.cpp
        tests/test_rules_determiner.cpp
        tests/test_rules_pronoun_case.cpp
        tests/test_rules_double_modal.cpp
        tests/test_rules_double_negative.cpp
        tests/test_rules_regionalisms.cpp
        tests/test_rule_cases.cpp
        tests/test_engine.cpp
        tests/test_corpus.cpp
        tests/test_regression.cpp
        tests/test_mutations.cpp
        tests/test_performance.cpp
    )
    target_link_libraries(test_stoppard PRIVATE stoppard GTest::gtest_main)
    target_include_directories(test_stoppard PRIVATE src)
    target_compile_definitions(test_stoppard PRIVATE
        TESTS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    gtest_discover_tests(test_stoppard)
endif()
