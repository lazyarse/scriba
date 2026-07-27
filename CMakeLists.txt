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
    src/CssHighlighter.cpp
    src/ExportPdfDialog.cpp
    src/DocxExporter.cpp
    src/HtmlToOoxml.cpp
    src/FindDialog.cpp
    src/TableDialog.cpp
    src/VegaLiteDialog.cpp
    src/EmojiDialog.cpp
    src/AboutDialog.cpp
    src/LogWindow.cpp
    src/MermaidPieDialog.cpp
    src/MermaidDialogBase.cpp
    src/MermaidFlowchartDialog.cpp
    src/MermaidSequenceDialog.cpp
    src/MermaidGanttDialog.cpp
    src/MermaidClassDialog.cpp
    src/MermaidErDialog.cpp
    src/MermaidStateDialog.cpp
    src/MermaidMindmapDialog.cpp
    src/MermaidTimelineDialog.cpp
    src/MermaidJourneyDialog.cpp
    src/MermaidQuadrantDialog.cpp
    src/MermaidSankeyDialog.cpp
    src/KatexHelperDialog.cpp
    src/StaticHelpers.cpp
    src/JsSnippets.cpp
    src/JsRenderEngine.cpp
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
    Qt6::PrintSupport
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
        src/JsSnippets.cpp
        vendor/md4c/md4c.c
        vendor/md4c/entity.c
    )
    target_include_directories(scriba_core PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/md4c
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

    function(add_mermaid_test name)
        string(TOLOWER "${name}" lname)
        add_executable(test_mermaid_${lname}_dialog
            tests/test_mermaid_${lname}_dialog.cpp
            src/Mermaid${name}Dialog.cpp
            src/MermaidDialogBase.cpp
            src/Preview.cpp)
        target_link_libraries(test_mermaid_${lname}_dialog
            PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
        add_test(NAME test_mermaid_${lname}_dialog
            COMMAND ${_TEST_PREFIX} ./test_mermaid_${lname}_dialog)
    endfunction()

    add_mermaid_test(Pie)
    add_mermaid_test(Flowchart)
    add_mermaid_test(Sankey)
    add_mermaid_test(Timeline)
    add_mermaid_test(Quadrant)
    add_mermaid_test(Journey)
    add_mermaid_test(State)
    add_mermaid_test(Sequence)
    add_mermaid_test(Gantt)
    add_mermaid_test(Mindmap)
    add_mermaid_test(Er)
    add_mermaid_test(Class)

    add_executable(test_katex_helper_dialog
        tests/test_katex_helper_dialog.cpp
        src/KatexHelperDialog.cpp
        src/Preview.cpp)
    target_link_libraries(test_katex_helper_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_katex_helper_dialog COMMAND ${_TEST_PREFIX} ./test_katex_helper_dialog)

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
        src/ExportPdfDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp
        src/FindDialog.cpp
        src/LogWindow.cpp
    src/MermaidPieDialog.cpp
    src/MermaidDialogBase.cpp
    src/MermaidFlowchartDialog.cpp
    src/MermaidSequenceDialog.cpp
    src/MermaidGanttDialog.cpp
    src/MermaidClassDialog.cpp
    src/MermaidErDialog.cpp
    src/MermaidStateDialog.cpp
    src/MermaidMindmapDialog.cpp
    src/MermaidTimelineDialog.cpp
    src/MermaidJourneyDialog.cpp
    src/MermaidQuadrantDialog.cpp
    src/MermaidSankeyDialog.cpp
    src/KatexHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_scroll_sync PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(test_scroll_sync PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
    target_link_libraries(test_scroll_sync PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test)
    add_test(NAME test_scroll_sync COMMAND ${_TEST_PREFIX} ./test_scroll_sync)

    add_executable(test_auto_save tests/test_auto_save.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidPieDialog.cpp src/MermaidDialogBase.cpp src/MermaidFlowchartDialog.cpp
        src/MermaidSequenceDialog.cpp src/MermaidGanttDialog.cpp
        src/MermaidClassDialog.cpp src/MermaidErDialog.cpp
        src/MermaidStateDialog.cpp src/MermaidMindmapDialog.cpp
        src/MermaidTimelineDialog.cpp src/MermaidJourneyDialog.cpp
        src/MermaidQuadrantDialog.cpp src/MermaidSankeyDialog.cpp
        src/KatexHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_auto_save PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_auto_save PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test)
    add_test(NAME test_auto_save COMMAND ${_TEST_PREFIX} ./test_auto_save)

    add_executable(test_find_dialog tests/test_find_dialog.cpp
        src/MainWindow.cpp src/Editor.cpp src/Preview.cpp
        src/CssConfig.cpp src/CssLoader.cpp
        src/PreferencesDialog.cpp src/CssEditorDialog.cpp src/CssHighlighter.cpp
        src/ExportPdfDialog.cpp src/JsRenderEngine.cpp src/DocxExporter.cpp src/HtmlToOoxml.cpp src/VegaLiteDialog.cpp
        src/EmojiDialog.cpp src/AboutDialog.cpp src/FindDialog.cpp
        src/LogWindow.cpp
        src/MermaidPieDialog.cpp src/MermaidDialogBase.cpp src/MermaidFlowchartDialog.cpp
        src/MermaidSequenceDialog.cpp src/MermaidGanttDialog.cpp
        src/MermaidClassDialog.cpp src/MermaidErDialog.cpp
        src/MermaidStateDialog.cpp src/MermaidMindmapDialog.cpp
        src/MermaidTimelineDialog.cpp src/MermaidJourneyDialog.cpp
        src/MermaidQuadrantDialog.cpp src/MermaidSankeyDialog.cpp
        src/KatexHelperDialog.cpp
        src/TableDialog.cpp
        resources/scriba.qrc)
    target_include_directories(test_find_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_find_dialog PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Svg Qt6::Test)
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
endif()

