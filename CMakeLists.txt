cmake_minimum_required(VERSION 3.16)
project(scriba VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets Svg)

add_subdirectory(vendor/mathml2omml)

# Vendored stoppard grammar engine (see vendor/stoppard). Scriba never builds or
# fetches its gtest suite — the option is forced OFF as a safety net.
set(STOPPARD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/stoppard)

add_executable(scriba WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/Editor.cpp
    src/Gutter.cpp
    src/Preview.cpp
    src/MarkdownParser.cpp
    src/MarkdownChecker.cpp
    src/MdRenderer.cpp
    src/Typography.cpp
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
    src/HtmlToMarkdown.cpp
    src/FindDialog.cpp
    src/TableDialog.cpp
    src/ChartDialog.cpp
    src/StockChartDialog.cpp
    src/EChartsParser.cpp
    src/MermaidParser.cpp
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
    src/CsvReader.cpp
    src/CsvColumnMapDialog.cpp
    src/SpellChecker.cpp
    src/SpellCheckDialog.cpp
    src/SpellHighlighter.cpp
    src/LinkValidator.cpp
    src/GrammarChecker.cpp
    src/StoppardEngine.cpp
    src/ValidationReport.cpp
    src/ValidationReportDialog.cpp
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
    stoppard
)

add_custom_command(TARGET scriba POST_BUILD
    COMMENT "Removing cached stylesheets from config directory"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$ENV{HOME}/.config/scriba/preview-base.css"
        "$ENV{HOME}/.config/scriba/print-base.css"
)

install(TARGETS scriba RUNTIME DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION share/doc/scriba)
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
set(CPACK_DEBIAN_PACKAGE_DEFAULT_PERMISSIONS TRUE)

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Scriba")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\Scriba.lnk' '$INSTDIR\\\\bin\\\\scriba.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\Scriba.lnk'")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "WriteRegStr HKCR '.md' '' 'Scriba.Markdown'
         WriteRegStr HKCR 'Scriba.Markdown' '' 'Scriba Markdown Document'
         WriteRegStr HKCR 'Scriba.Markdown\\\\DefaultIcon' '' '$INSTDIR\\\\icons\\\\scriba.ico'
         WriteRegStr HKCR 'Scriba.Markdown\\\\shell\\\\open\\\\command' '' '$INSTDIR\\\\bin\\\\scriba.exe \\\"%1\\\"'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "DeleteRegKey HKCR 'Scriba.Markdown'
         Delete '$SMPROGRAMS\\\\Scriba.lnk'")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/resources/icons/scriba.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icons/scriba.ico")
    set(CPACK_NSIS_MUI_UNICON "${CMAKE_SOURCE_DIR}/resources/icons/scriba.ico")
    install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/scriba.ico
        DESTINATION icons)
    install(CODE "
        execute_process(COMMAND windeployqt --release --no-compiler-runtime
            \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/scriba.exe\"
            WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX})
    ")
endif()

include(CPack)

target_compile_definitions(scriba PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        if(WIN32)
            message(FATAL_ERROR
                "GTest not found. Install it (e.g. vcpkg install gtest and pass "
                "-DCMAKE_TOOLCHAIN_FILE, or build googletest and set gtest_DIR).")
        else()
            message(FATAL_ERROR
                "GTest not found. Install it, e.g. sudo apt install libgtest-dev")
        endif()
    endif()

    enable_testing()

    find_program(_XVFB_RUN xvfb-run)
    if(_XVFB_RUN)
        set(_TEST_PREFIX ${_XVFB_RUN} -a)
    endif()

    add_library(scriba_core STATIC
        src/MarkdownParser.cpp
        src/MdRenderer.cpp
        src/Typography.cpp
        src/CssUtils.cpp
        src/StaticHelpers.cpp
        src/Readability.cpp
        src/JsSnippets.cpp
        src/CsvReader.cpp
        src/EChartsParser.cpp
        src/MermaidParser.cpp
        vendor/md4c/src/md4c.c
        vendor/md4c/src/entity.c
    )
    target_include_directories(scriba_core PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/md4c/src
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(scriba_core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets)

    add_library(scriba_spell STATIC
        src/SpellChecker.cpp
        src/SpellHighlighter.cpp
        src/LinkValidator.cpp
        src/MarkdownChecker.cpp
        src/GrammarChecker.cpp
        src/StoppardEngine.cpp
        src/ValidationReport.cpp
        src/ValidationReportDialog.cpp
    )
    target_include_directories(scriba_spell PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(scriba_spell PUBLIC scriba_core stoppard Qt6::Network)

    add_library(test_editor_helpers STATIC
        tests/EditorTestHarness.cpp
        tests/EditorTestHarness.h
    )
    target_include_directories(test_editor_helpers PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_editor_helpers PUBLIC
        GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core
    )

    add_library(scriba_test_main STATIC
        tests/test_main.cpp
        tests/TestConfig.h
    )
    target_include_directories(scriba_test_main PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(scriba_test_main PUBLIC
        GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets
    )

    add_executable(test_markdown_parser tests/test_markdown_parser.cpp)
    target_link_libraries(test_markdown_parser PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_markdown_parser COMMAND ${_TEST_PREFIX} ./test_markdown_parser)

    add_executable(test_md_renderer_helpers tests/test_md_renderer_helpers.cpp)
    target_link_libraries(test_md_renderer_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_md_renderer_helpers COMMAND ${_TEST_PREFIX} ./test_md_renderer_helpers)

    add_executable(test_chartsource tests/test_chartsource.cpp)
    target_link_libraries(test_chartsource PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_chartsource COMMAND ${_TEST_PREFIX} ./test_chartsource)

    add_executable(test_typography tests/test_typography.cpp)
    target_link_libraries(test_typography PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_typography COMMAND ${_TEST_PREFIX} ./test_typography)

    add_executable(test_css_utils tests/test_css_utils.cpp)
    target_link_libraries(test_css_utils PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_css_utils COMMAND ${_TEST_PREFIX} ./test_css_utils)

    add_executable(test_css_highlighter tests/test_css_highlighter.cpp src/CssHighlighter.cpp)
    target_include_directories(test_css_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_highlighter PRIVATE scriba_test_main)
    add_test(NAME test_css_highlighter COMMAND ${_TEST_PREFIX} ./test_css_highlighter)

    add_executable(test_static_helpers tests/test_static_helpers.cpp resources/scriba.qrc resources/twemoji-svg.qrc)
    target_link_libraries(test_static_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_static_helpers COMMAND ${_TEST_PREFIX} ./test_static_helpers)

    add_executable(test_spell_checker tests/test_spell_checker.cpp resources/scriba.qrc)
    target_link_libraries(test_spell_checker PRIVATE GTest::gtest scriba_spell)
    add_test(NAME test_spell_checker COMMAND ${_TEST_PREFIX} ./test_spell_checker)

    add_executable(test_spell_highlighter tests/test_spell_highlighter.cpp resources/scriba.qrc)
    target_include_directories(test_spell_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_spell_highlighter PRIVATE GTest::gtest scriba_spell Qt6::Test)
    add_test(NAME test_spell_highlighter COMMAND ${_TEST_PREFIX} ./test_spell_highlighter)

    add_executable(test_grammar_checker tests/test_grammar_checker.cpp)
    target_include_directories(test_grammar_checker PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_grammar_checker PRIVATE scriba_test_main scriba_spell)
    add_test(NAME test_grammar_checker COMMAND ${_TEST_PREFIX} ./test_grammar_checker)

    add_executable(test_link_validator tests/test_link_validator.cpp)
    target_include_directories(test_link_validator PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_link_validator PRIVATE scriba_test_main scriba_spell)
    add_test(NAME test_link_validator COMMAND ${_TEST_PREFIX} ./test_link_validator)

    add_executable(test_broken_links tests/test_broken_links.cpp)
    target_include_directories(test_broken_links PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_broken_links PRIVATE GTest::gtest scriba_spell Qt6::Test)
    add_test(NAME test_broken_links COMMAND ${_TEST_PREFIX} ./test_broken_links)

    add_executable(test_markdown_checker tests/test_markdown_checker.cpp)
    target_include_directories(test_markdown_checker PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_markdown_checker PRIVATE scriba_core GTest::gtest_main scriba_spell)
    add_test(NAME test_markdown_checker COMMAND ${_TEST_PREFIX} ./test_markdown_checker)

    add_executable(test_validation_report tests/test_validation_report.cpp resources/scriba.qrc)
    target_include_directories(test_validation_report PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_validation_report PRIVATE GTest::gtest scriba_spell Qt6::Test)
    add_test(NAME test_validation_report COMMAND ${_TEST_PREFIX} ./test_validation_report)

    add_executable(test_validation_report_dialog tests/test_validation_report_dialog.cpp)
    target_include_directories(test_validation_report_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_validation_report_dialog PRIVATE scriba_test_main scriba_spell)
    add_test(NAME test_validation_report_dialog COMMAND ${_TEST_PREFIX} ./test_validation_report_dialog)

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
        src/CsvColumnMapDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_mermaid_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_mermaid_dialog
        COMMAND ${_TEST_PREFIX} ./test_mermaid_dialog)

    add_executable(test_chart_dialog
        tests/test_chart_dialog.cpp
        src/ChartDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_chart_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_chart_dialog COMMAND ${_TEST_PREFIX} ./test_chart_dialog)

    add_executable(test_stock_chart_dialog
        tests/test_stock_chart_dialog.cpp
        src/StockChartDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_stock_chart_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_stock_chart_dialog COMMAND ${_TEST_PREFIX} ./test_stock_chart_dialog)

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
    target_link_libraries(test_editor_autocomplete PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_editor_autocomplete COMMAND ${_TEST_PREFIX} ./test_editor_autocomplete)

    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_editor_completion_ui tests/test_editor_completion_ui.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc resources/twemoji-svg.qrc)
    target_include_directories(test_editor_completion_ui PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_completion_ui PRIVATE GTest::gtest test_editor_helpers Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core scriba_spell)
    add_test(NAME test_editor_completion_ui COMMAND ${_TEST_PREFIX} ./test_editor_completion_ui)

    add_executable(test_folding tests/test_folding.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_folding PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_folding PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core scriba_spell)
    add_test(NAME test_folding COMMAND ${_TEST_PREFIX} ./test_folding)

    add_executable(test_gutter_line_height tests/test_gutter_line_height.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_gutter_line_height PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_gutter_line_height PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core scriba_spell)
    add_test(NAME test_gutter_line_height COMMAND ${_TEST_PREFIX} ./test_gutter_line_height)

    add_executable(test_gutter_width tests/test_gutter_width.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_gutter_width PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_gutter_width PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_core scriba_spell)
    add_test(NAME test_gutter_width COMMAND ${_TEST_PREFIX} ./test_gutter_width)

    add_executable(test_editor_typing tests/test_editor_typing.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_editor_typing PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_typing PRIVATE GTest::gtest test_editor_helpers scriba_spell)
    add_test(NAME test_editor_typing COMMAND ${_TEST_PREFIX} ./test_editor_typing)

    add_executable(test_editor_autocorrect tests/test_editor_autocorrect.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_editor_autocorrect PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_autocorrect PRIVATE GTest::gtest test_editor_helpers scriba_spell)
    add_test(NAME test_editor_autocorrect COMMAND ${_TEST_PREFIX} ./test_editor_autocorrect)

    add_executable(test_editor_wrap tests/test_editor_wrap.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_editor_wrap PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_wrap PRIVATE GTest::gtest test_editor_helpers scriba_spell)
    add_test(NAME test_editor_wrap COMMAND ${_TEST_PREFIX} ./test_editor_wrap)

    add_executable(test_enter_scroll tests/test_enter_scroll.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_enter_scroll PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_enter_scroll PRIVATE GTest::gtest test_editor_helpers scriba_spell)
    add_test(NAME test_enter_scroll COMMAND ${_TEST_PREFIX} ./test_enter_scroll)

    add_executable(test_underline_overlay tests/test_underline_overlay.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_underline_overlay PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_underline_overlay PRIVATE GTest::gtest scriba_core scriba_spell)
    add_test(NAME test_underline_overlay COMMAND ${_TEST_PREFIX} ./test_underline_overlay)

    add_executable(test_spell_check_dialog tests/test_spell_check_dialog.cpp
        src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp resources/scriba.qrc)
    target_include_directories(test_spell_check_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_spell_check_dialog PRIVATE
        GTest::gtest scriba_core scriba_spell Qt6::Test Qt6::Widgets)
    add_test(NAME test_spell_check_dialog COMMAND ${_TEST_PREFIX} ./test_spell_check_dialog)

    add_executable(test_scroll_sync tests/test_scroll_sync.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp
        src/FindDialog.cpp
        src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        src/HtmlToMarkdown.cpp
        resources/scriba.qrc)
    target_include_directories(test_scroll_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_scroll_sync PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
    target_link_libraries(test_scroll_sync PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_scroll_sync COMMAND ${_TEST_PREFIX} ./test_scroll_sync)

    add_executable(test_html_to_markdown
        tests/test_html_to_markdown.cpp
        src/HtmlToMarkdown.cpp
        resources/scriba.qrc)
    target_include_directories(test_html_to_markdown PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_html_to_markdown
        PRIVATE GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_html_to_markdown COMMAND ${_TEST_PREFIX} ./test_html_to_markdown)

    add_executable(test_anchor_navigation tests/test_anchor_navigation.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp
        src/FindDialog.cpp
        src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_anchor_navigation PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_anchor_navigation PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
    target_link_libraries(test_anchor_navigation PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_anchor_navigation COMMAND ${_TEST_PREFIX} ./test_anchor_navigation)

    add_executable(test_emoji_preview tests/test_emoji_preview.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp
        src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc
        resources/twemoji-svg.qrc)
    target_include_directories(test_emoji_preview PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_emoji_preview PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
    target_link_libraries(test_emoji_preview PRIVATE
        scriba_core GTest::gtest test_editor_helpers Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_emoji_preview COMMAND ${_TEST_PREFIX} ./test_emoji_preview)

    add_executable(test_auto_save tests/test_auto_save.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_auto_save PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_auto_save PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_auto_save COMMAND ${_TEST_PREFIX} ./test_auto_save)

    add_executable(test_multi_tab_typing tests/test_multi_tab_typing.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_multi_tab_typing PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_multi_tab_typing PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_multi_tab_typing COMMAND ${_TEST_PREFIX} ./test_multi_tab_typing)

    add_executable(test_dirty_on_load tests/test_dirty_on_load.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_dirty_on_load PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_dirty_on_load PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_dirty_on_load COMMAND ${_TEST_PREFIX} ./test_dirty_on_load)

    add_executable(test_editor_initial_css tests/test_editor_initial_css.cpp
    src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
    src/CssConfig.cpp src/CssLoader.cpp
    src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
    src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
    src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
    src/LogWindow.cpp
    src/MermaidDialog.cpp
    src/KatexHelperDialog.cpp
    src/MchemHelperDialog.cpp
    src/CsvColumnMapDialog.cpp
    src/TableDialog.cpp
    resources/scriba.qrc)
target_include_directories(test_editor_initial_css PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_editor_initial_css PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
target_link_libraries(test_editor_initial_css PRIVATE
    scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
add_test(NAME test_editor_initial_css COMMAND ${_TEST_PREFIX} ./test_editor_initial_css)

add_executable(test_find_dialog tests/test_find_dialog.cpp
        src/MainWindow.cpp src/HtmlToMarkdown.cpp src/SpellCheckDialog.cpp src/Editor.cpp src/Gutter.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/ExportHtmlDialog.cpp src/ExportDocxDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/UnitConverter.cpp src/CssValueParser.cpp src/ChartDialog.cpp src/StockChartDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidDialog.cpp
        src/KatexHelperDialog.cpp
        src/MchemHelperDialog.cpp
        src/CsvColumnMapDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_find_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_find_dialog PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test mathml2omml scriba_spell)
    add_test(NAME test_find_dialog COMMAND ${_TEST_PREFIX} ./test_find_dialog)

    add_executable(test_print_css_file tests/test_print_css_file.cpp)
    target_compile_definitions(test_print_css_file PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(test_print_css_file PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_print_css_file COMMAND ${_TEST_PREFIX} ./test_print_css_file)

    add_executable(test_css_loader tests/test_css_loader.cpp
        src/CssLoader.cpp src/CssConfig.cpp resources/scriba.qrc)
    target_include_directories(test_css_loader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_loader PRIVATE scriba_core scriba_test_main Qt6::Core)
    add_test(NAME test_css_loader COMMAND ${_TEST_PREFIX} ./test_css_loader)

    add_executable(test_settings_migration tests/test_settings_migration.cpp)
    target_include_directories(test_settings_migration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_settings_migration PRIVATE scriba_core scriba_test_main Qt6::Core)
    add_test(NAME test_settings_migration COMMAND ${_TEST_PREFIX} ./test_settings_migration)

    add_executable(test_css_config tests/test_css_config.cpp src/CssConfig.cpp)
    target_include_directories(test_css_config PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_config PRIVATE scriba_core scriba_test_main Qt6::Core)
    add_test(NAME test_css_config COMMAND ${_TEST_PREFIX} ./test_css_config)

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

    add_executable(test_csv_reader tests/test_csv_reader.cpp src/CsvReader.cpp)
    target_include_directories(test_csv_reader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_csv_reader PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_csv_reader COMMAND ${_TEST_PREFIX} ./test_csv_reader)

    # WebEngine-spawning suites must not fork concurrently: concurrent WebEngine
    # processes cause flaky failures. Serialize them against each other (via
    # RESOURCE_LOCK) under `ctest -jN` while non-WebEngine tests run in parallel.
    set_tests_properties(
        test_mermaid_dialog test_chart_dialog test_stock_chart_dialog
        test_katex_helper_dialog test_mchem_helper_dialog
        test_scroll_sync test_html_to_markdown test_anchor_navigation
        test_emoji_preview test_auto_save test_multi_tab_typing
        test_dirty_on_load test_editor_initial_css test_find_dialog
        test_pdf_export test_export_html test_docx_image_export
        PROPERTIES RESOURCE_LOCK webkit)
endif()

