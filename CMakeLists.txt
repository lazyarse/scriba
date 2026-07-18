cmake_minimum_required(VERSION 3.16)
project(scriba VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets Svg)

add_executable(scriba
    src/main.cpp
    src/MainWindow.cpp
    src/Editor.cpp
    src/Preview.cpp
    src/MarkdownParser.cpp
    src/MdRenderer.cpp
    src/CssConfig.cpp
    src/CssLoader.cpp
    src/CssUtils.cpp
    src/PreferencesDialog.cpp
    src/CssEditorDialog.cpp
    src/ExportPdfDialog.cpp
    src/FindDialog.cpp
    src/VegaLiteDialog.cpp
    src/StaticHelpers.cpp
    vendor/md4c/md4c.c
    vendor/md4c/md4c-html.c
    vendor/md4c/entity.c
    resources/scriba.qrc
    resources/twemoji-svg.qrc
)

target_include_directories(scriba PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/md4c
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scriba PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::Svg
)

add_custom_command(TARGET scriba POST_BUILD
    COMMENT "Removing cached stylesheets from config directory"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$ENV{HOME}/.config/Scriba/Scriba/editor-base.css"
        "$ENV{HOME}/.config/Scriba/Scriba/preview-base.css"
)

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    find_package(GTest REQUIRED)

    enable_testing()

    add_library(scriba_core STATIC
        src/MarkdownParser.cpp
        src/MdRenderer.cpp
        src/CssUtils.cpp
        src/StaticHelpers.cpp
        vendor/md4c/md4c.c
        vendor/md4c/entity.c
    )
    target_include_directories(scriba_core PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/md4c
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(scriba_core PUBLIC Qt6::Core Qt6::Gui)

    add_executable(test_markdown_parser tests/test_markdown_parser.cpp)
    target_link_libraries(test_markdown_parser PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_markdown_parser COMMAND test_markdown_parser)

    add_executable(test_md_renderer_helpers tests/test_md_renderer_helpers.cpp)
    target_link_libraries(test_md_renderer_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_md_renderer_helpers COMMAND test_md_renderer_helpers)

    add_executable(test_css_utils tests/test_css_utils.cpp)
    target_link_libraries(test_css_utils PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_css_utils COMMAND test_css_utils)

    add_executable(test_static_helpers tests/test_static_helpers.cpp)
    target_link_libraries(test_static_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_static_helpers COMMAND test_static_helpers)

    add_executable(test_editor_list_continuation tests/test_editor_list_continuation.cpp)
    target_link_libraries(test_editor_list_continuation PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_editor_list_continuation COMMAND test_editor_list_continuation)

    add_executable(test_cursor_restore tests/test_cursor_restore.cpp)
    target_link_libraries(test_cursor_restore PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_cursor_restore COMMAND test_cursor_restore)

    add_executable(test_katex_integration tests/test_katex_integration.cpp)
    target_compile_definitions(test_katex_integration PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources")
    target_link_libraries(test_katex_integration PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_katex_integration COMMAND test_katex_integration)

    add_executable(test_readability tests/test_readability.cpp)
    target_link_libraries(test_readability PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_readability COMMAND test_readability)

    add_executable(test_vegalite_dialog tests/test_vegalite_dialog.cpp src/VegaLiteDialog.cpp src/Preview.cpp)
    target_link_libraries(test_vegalite_dialog PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_vegalite_dialog COMMAND test_vegalite_dialog)

    add_executable(test_editor_autocomplete tests/test_editor_autocomplete.cpp)
    target_link_libraries(test_editor_autocomplete PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_editor_autocomplete COMMAND test_editor_autocomplete)

    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_editor_completion_ui tests/test_editor_completion_ui.cpp src/Editor.cpp)
    target_include_directories(test_editor_completion_ui PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_completion_ui PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test scriba_core)
    add_test(NAME test_editor_completion_ui COMMAND test_editor_completion_ui)

    add_executable(test_scroll_sync tests/test_scroll_sync.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp
        src/ExportPdfDialog.cpp src/VegaLiteDialog.cpp
        src/FindDialog.cpp)
    target_include_directories(test_scroll_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_scroll_sync PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test)
    add_test(NAME test_scroll_sync COMMAND test_scroll_sync)
endif()

