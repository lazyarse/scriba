cmake_minimum_required(VERSION 3.16)
project(scriba VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets Svg)

include(FetchContent)
FetchContent_Declare(mathml2omml
    GIT_REPOSITORY https://github.com/lazyarse/mathml2omml.git
    GIT_TAG v2.0.1)
FetchContent_MakeAvailable(mathml2omml)

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
    src/CssHighlighter.cpp
    src/ExportPdfDialog.cpp
    src/ExportHtmlDialog.cpp
    src/ExportDocxDialog.cpp
    src/DocxExporter.cpp
    src/HtmlToOoxml.cpp
    src/FindDialog.cpp
    src/TableDialog.cpp
    src/VegaLiteDialog.cpp
    src/EmojiDialog.cpp
    src/AboutDialog.cpp
    src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
    src/StaticHelpers.cpp
    src/Readability.h
    src/Readability.cpp
    src/JsSnippets.cpp
    src/JsRenderEngine.cpp
    src/UnitConverter.cpp
    src/CssValueParser.cpp
    vendor/md4c/src/md4c.c
    vendor/md4c/src/md4c-html.c
    vendor/md4c/src/entity.c
    resources/scriba.qrc
    resources/twemoji-svg.qrc
)

target_include_directories(scriba PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/md4c/src
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scriba PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::Svg
    Qt6::PrintSupport
    mathml2omml
)

add_custom_command(TARGET scriba POST_BUILD
    COMMENT "Removing cached stylesheets from config directory"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$ENV{HOME}/.config/scriba/preview-base.css"
)

install(TARGETS scriba RUNTIME DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/scriba.svg
    DESTINATION share/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_SOURCE_DIR}/resources/scriba.desktop
    DESTINATION share/applications)

set(CPACK_PACKAGE_NAME "scriba")
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(GIT_VERSION)
    string(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_VERSION}")
    set(CPACK_PACKAGE_VERSION "${GIT_VERSION}")
else()
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt6-based Markdown editor with charts, latex support, code highlighting, pdf export, and configurable")
set(CPACK_PACKAGE_CONTACT "LazyArse <lazyarse000@protonmail.com>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

target_compile_definitions(scriba PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    find_package(GTest REQUIRED)

    enable_testing()

    find_program(_XVFB_RUN xvfb-run)
    if(_XVFB_RUN)
        set(_TEST_PREFIX ${_XVFB_RUN} -a)
    endif()

    add_library(scriba_core STATIC
        src/MarkdownParser.cpp
        src/MdRenderer.cpp
        src/CssUtils.cpp
        src/StaticHelpers.cpp
        src/Readability.cpp
        src/JsSnippets.cpp
        vendor/md4c/src/md4c.c
        vendor/md4c/src/entity.c
    )
    target_include_directories(scriba_core PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/md4c/src
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(scriba_core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets)

    add_executable(test_markdown_parser tests/test_markdown_parser.cpp)
    target_link_libraries(test_markdown_parser PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_markdown_parser COMMAND ${_TEST_PREFIX} ./test_markdown_parser)

    add_executable(test_md_renderer_helpers tests/test_md_renderer_helpers.cpp)
    target_link_libraries(test_md_renderer_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_md_renderer_helpers COMMAND ${_TEST_PREFIX} ./test_md_renderer_helpers)

    add_executable(test_css_utils tests/test_css_utils.cpp)
    target_link_libraries(test_css_utils PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_css_utils COMMAND ${_TEST_PREFIX} ./test_css_utils)

    add_executable(test_static_helpers tests/test_static_helpers.cpp)
    target_link_libraries(test_static_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_static_helpers COMMAND ${_TEST_PREFIX} ./test_static_helpers)

    add_executable(test_editor_list_continuation tests/test_editor_list_continuation.cpp)
    target_link_libraries(test_editor_list_continuation PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_editor_list_continuation COMMAND ${_TEST_PREFIX} ./test_editor_list_continuation)

    add_executable(test_cursor_restore tests/test_cursor_restore.cpp)
    target_link_libraries(test_cursor_restore PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_cursor_restore COMMAND ${_TEST_PREFIX} ./test_cursor_restore)

    add_executable(test_katex_integration tests/test_katex_integration.cpp)
    target_compile_definitions(test_katex_integration PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources")
    target_link_libraries(test_katex_integration PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_katex_integration COMMAND ${_TEST_PREFIX} ./test_katex_integration)

    add_executable(test_readability tests/test_readability.cpp)
    target_link_libraries(test_readability PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_readability COMMAND ${_TEST_PREFIX} ./test_readability)

    add_executable(test_mermaid_dialog
        tests/test_mermaid_dialog.cpp
        src/MermaidDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_mermaid_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_mermaid_dialog
        COMMAND ${_TEST_PREFIX} ./test_mermaid_dialog)

    add_executable(test_katex_helper_dialog
        tests/test_katex_helper_dialog.cpp
        src/KatexHelperDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_katex_helper_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_katex_helper_dialog COMMAND ${_TEST_PREFIX} ./test_katex_helper_dialog)

    add_executable(test_mchem_helper_dialog
        tests/test_mchem_helper_dialog.cpp
        src/MchemHelperDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_mchem_helper_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_mchem_helper_dialog COMMAND ${_TEST_PREFIX} ./test_mchem_helper_dialog)

    add_executable(test_editor_autocomplete tests/test_editor_autocomplete.cpp)
    target_link_libraries(test_editor_autocomplete PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_editor_autocomplete COMMAND ${_TEST_PREFIX} ./test_editor_autocomplete)

    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_editor_completion_ui tests/test_editor_completion_ui.cpp src/Editor.cpp resources/scriba.qrc)
    target_include_directories(test_editor_completion_ui PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_completion_ui PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core)
    add_test(NAME test_editor_completion_ui COMMAND ${_TEST_PREFIX} ./test_editor_completion_ui)

    add_executable(test_scroll_sync tests/test_scroll_sync.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp
        src/FindDialog.cpp
        src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_scroll_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_scroll_sync PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
    target_link_libraries(test_scroll_sync PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml)
    add_test(NAME test_scroll_sync COMMAND ${_TEST_PREFIX} ./test_scroll_sync)

    add_executable(test_auto_save tests/test_auto_save.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_auto_save PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_auto_save PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml)
    add_test(NAME test_auto_save COMMAND ${_TEST_PREFIX} ./test_auto_save)

    add_executable(test_editor_initial_css tests/test_editor_initial_css.cpp
    src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
    src/CssConfig.cpp src/CssLoader.cpp
    src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
    src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/VegaLiteDialog.cpp
    src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
    src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
    src/TableDialog.cpp
    resources/scriba.qrc)
target_include_directories(test_editor_initial_css PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_editor_initial_css PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
target_link_libraries(test_editor_initial_css PRIVATE
    scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml)
add_test(NAME test_editor_initial_css COMMAND ${_TEST_PREFIX} ./test_editor_initial_css)

add_executable(test_find_dialog tests/test_find_dialog.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_find_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_find_dialog PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml)
    add_test(NAME test_find_dialog COMMAND ${_TEST_PREFIX} ./test_find_dialog)

    add_executable(test_print_css_file tests/test_print_css_file.cpp)
    target_compile_definitions(test_print_css_file PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(test_print_css_file PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_print_css_file COMMAND ${_TEST_PREFIX} ./test_print_css_file)

    add_executable(test_pdf_export tests/test_pdf_export.cpp
        src/ExportPdfDialog.cpp src/JsRenderEngine.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        resources/scriba.qrc)
    target_include_directories(test_pdf_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_pdf_export PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::PrintSupport Qt6::Test)
    add_test(NAME test_pdf_export COMMAND ${_TEST_PREFIX} ./test_pdf_export)

    add_executable(test_export_html tests/test_export_html.cpp
        src/ExportHtmlDialog.cpp src/JsRenderEngine.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        resources/scriba.qrc)
    target_include_directories(test_export_html PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_export_html PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Network Qt6::Test)
    add_test(NAME test_export_html COMMAND ${_TEST_PREFIX} ./test_export_html)

    add_executable(test_docx_export tests/test_docx_export.cpp
        src/HtmlToOoxml.cpp
        src/UnitConverter.cpp
        src/CssValueParser.cpp)
    target_include_directories(test_docx_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_docx_export PRIVATE
        scriba_core GTest::gtest Qt6::Gui Qt6::Svg mathml2omml)
    add_test(NAME test_docx_export COMMAND ${_TEST_PREFIX} ./test_docx_export)

    add_executable(test_docx_image_export tests/test_docx_image_export.cpp
        src/JsRenderEngine.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp src/HtmlToOoxml.cpp
        src/UnitConverter.cpp
        src/CssValueParser.cpp
        resources/scriba.qrc)
    target_include_directories(test_docx_image_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_docx_image_export PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Network Qt6::Svg mathml2omml)
    add_test(NAME test_docx_image_export COMMAND ${_TEST_PREFIX} ./test_docx_image_export)

    add_executable(test_mchem_integration tests/test_mchem_integration.cpp)
    target_compile_definitions(test_mchem_integration PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources")
    target_link_libraries(test_mchem_integration PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_mchem_integration COMMAND ${_TEST_PREFIX} ./test_mchem_integration)
endif()

