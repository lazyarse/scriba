cmake_minimum_required(VERSION 3.16)
project(scriba VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebEngineWidgets Svg WebChannel)

add_subdirectory(vendor/mathml2omml)
add_subdirectory(vendor/miniz)

# Vendored stoppard grammar engine (see vendor/stoppard). Scriba never builds or
# fetches its gtest suite — the option is forced OFF as a safety net.
set(STOPPARD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/stoppard)

# Vendored libgit2 for the Mermaid Git Graph panel (see vendor/libgit2). Built
# statically with network/SSH disabled and bundled zlib/pcre2 so the tree
# compiles fully offline. LIBGIT2_EMBEDDED (an upstream libgit2 option, used by
# cmake/PkgBuildConfig.cmake) suppresses the pkgconfig install rules so they
# cannot pollute the scriba package.
#
# libgit2's options share names with CMake conventions and with scriba's own
# options (BUILD_TESTS), so its values are forced here and scriba's values are
# restored afterwards.
set(_scriba_BUILD_TESTS "${BUILD_TESTS}")
set(_scriba_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CLI OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(USE_SSH OFF CACHE BOOL "" FORCE)
set(USE_HTTPS OFF CACHE BOOL "" FORCE)
set(USE_NSEC OFF CACHE BOOL "" FORCE)
set(USE_BUNDLED_ZLIB ON CACHE BOOL "" FORCE)
set(REGEX_BACKEND "builtin" CACHE STRING "" FORCE)
set(LIBGIT2_EMBEDDED ON)
add_subdirectory(vendor/libgit2)
if("${_scriba_BUILD_TESTS}" STREQUAL "")
    unset(BUILD_TESTS CACHE)
else()
    set(BUILD_TESTS "${_scriba_BUILD_TESTS}" CACHE BOOL "" FORCE)
endif()
if("${_scriba_BUILD_SHARED_LIBS}" STREQUAL "")
    unset(BUILD_SHARED_LIBS CACHE)
else()
    set(BUILD_SHARED_LIBS "${_scriba_BUILD_SHARED_LIBS}" CACHE BOOL "" FORCE)
endif()
unset(_scriba_BUILD_TESTS)
unset(_scriba_BUILD_SHARED_LIBS)

# Git Graph generator, used by the main app and by test targets that compile
# MermaidDialog.cpp. libgit2's public headers are only generated into the build
# tree, so the scriba include dir is exported for consumers.
add_library(scriba_gitgraph STATIC
    src/mermaid/GitGraphBuilder.cpp
)
target_include_directories(scriba_gitgraph PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/libgit2/include
    ${CMAKE_BINARY_DIR}/vendor/libgit2/include
)
target_link_libraries(scriba_gitgraph PRIVATE libgit2package Qt6::Core)

# Resource object libraries. The qrc-generated qrc_*.cpp registers its
# resources via an anonymous-namespace static initializer (qInitResources_*),
# which a static archive would silently drop at link time; as OBJECT libraries
# the objects are passed straight into every consuming executable, so resource
# registration always runs. Compiling each qrc once instead of once per target
# that listed it cuts duplicated 30 MB+ compile work out of the dev loop.
add_library(scriba_resources OBJECT resources/scriba.qrc)
target_link_libraries(scriba_resources PUBLIC Qt6::Core)

# Twemoji is split out on purpose: its generated object is ~8.5 MB and is only
# needed by the targets that render emoji, so it must not be pulled into every
# consumer of scriba_resources.
add_library(scriba_twemoji OBJECT resources/twemoji-svg.qrc)
target_link_libraries(scriba_twemoji PUBLIC Qt6::Core)

# App version — computed before scriba_app so the object lib can bake
# SCRIBA_VERSION into the app sources it compiles (only src/dialogs/AboutDialog.cpp
# consumes it). CPACK_PACKAGE_VERSION falls back to the project version when
# git is unavailable (e.g. a source tarball).
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

set(SCRIBA_GUTTER_SOURCES src/editor/Gutter.cpp)

set(SCRIBA_EDITOR_SOURCES src/editor/Editor.cpp
    src/editor/EditorTyping.cpp src/editor/EditorTable.cpp src/editor/EditorFolding.cpp
    src/editor/EditorCompletions.cpp src/editor/EditorMenu.cpp src/editor/EditorScrollBar.cpp
    src/editor/IssueSummaryPane.cpp
    ${SCRIBA_GUTTER_SOURCES})

set(SCRIBA_MAINWINDOW_SOURCES src/mainwindow/MainWindow.cpp
    src/mainwindow/MainWindow_Tabs.cpp src/mainwindow/MainWindow_File.cpp src/mainwindow/MainWindow_Preview.cpp
    src/mainwindow/MainWindow_Corpus.cpp src/mainwindow/MainWindow_Menu.cpp)

set(SCRIBA_PREFS_SOURCES src/prefs/PreferencesDialog.cpp src/prefs/PreferencesPages.cpp
    src/prefs/PreferencesPagesTheme.cpp src/prefs/PreferencesPagesContent.cpp
    src/prefs/PreferencesPagesSystem.cpp src/prefs/PreferencesPagesAppearance.cpp)

set(SCRIBA_MERMAID_SOURCES src/mermaid/MermaidDialog.cpp
    src/mermaid/MermaidDialog_Flowchart.cpp src/mermaid/MermaidDialog_Gantt.cpp
    src/mermaid/MermaidDialog_Class.cpp src/mermaid/MermaidDialog_State.cpp
    src/mermaid/MermaidDialog_Others.cpp)

set(SCRIBA_APP_WEBENGINE_SOURCES
    src/preview/PreviewPagination.cpp src/preview/PreviewRenderWorker.cpp
    ${SCRIBA_MAINWINDOW_SOURCES}
    src/corpus/Corpus.cpp src/corpus/CorpusWatcher.cpp src/corpus/LinkFixer.cpp src/corpus/CorpusIndex.cpp
    src/corpus/CorpusFilesPanel.cpp
    src/io/ZipReader.cpp src/io/OoxmlToHtml.cpp src/io/OoxmlConverter.cpp
    src/io/DocxImporter.cpp src/io/HtmlToMarkdown.cpp src/io/PdfImporter.cpp
    src/spell/SpellCheckDialog.cpp
    src/preview/Preview.cpp src/preview/PreviewBridge.cpp src/corpus/ExportCorpusDialog.cpp
    src/io/PdfRenderer.cpp src/css/CssConfig.cpp src/css/CssLoader.cpp
    src/css/CssEditorDialog.cpp src/css/CssHighlighter.cpp
    src/io/ExportPdfDialog.cpp src/io/ExportHtmlDialog.cpp src/io/ExportDocxDialog.cpp
    src/preview/JsRenderEngine.cpp src/io/DocxExporter.cpp src/io/HtmlToOoxml.cpp
    src/css/UnitConverter.cpp src/css/CssValueParser.cpp src/charts/ChartDialog.cpp
    src/charts/StockChartDialog.cpp src/charts/AdvancedChartDialog.cpp
    src/dialogs/EmojiDialog.cpp src/dialogs/AboutDialog.cpp src/dialogs/FindDialog.cpp src/dialogs/LogWindow.cpp
    ${SCRIBA_MERMAID_SOURCES} src/dialogs/KatexHelperDialog.cpp src/dialogs/MchemHelperDialog.cpp
    src/io/CsvColumnMapDialog.cpp src/dialogs/TableDialog.cpp)

# The app's WebEngine sources as one object library. Every full-app test target
# used to expand SCRIBA_APP_WEBENGINE_SOURCES and recompile all ~58 files from
# scratch; the scriba executable compiled them as a superset. With OBJECT
# libraries each file compiles once (in this lib) and every consumer links the
# same objects — identical runtime behaviour, a fraction of the dev-loop work.
# NOT a static library: objects with static-initializer side effects (emoji
# cache, qRegisterMetaType, ...) would be dropped from an archive.
# Resources come in via scriba_resources (qrc is out of the source list); note
# that object-library FILES do not propagate transitively, so each consumer of
# scriba_app must link scriba_resources (and scriba_twemoji) explicitly.
add_library(scriba_app OBJECT ${SCRIBA_APP_WEBENGINE_SOURCES})
target_include_directories(scriba_app PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(scriba_app PRIVATE SCRIBA_VERSION="${CPACK_PACKAGE_VERSION}")
target_link_libraries(scriba_app PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Svg Qt6::PrintSupport
    mathml2omml stoppard
    scriba_gitgraph miniz)

# Editor and Preferences sources get their own object libraries: the standalone
# editor/prefs test targets (test_folding, test_gutter_*, test_editor_*, ...)
# used to expand SCRIBA_EDITOR_SOURCES / SCRIBA_PREFS_SOURCES directly and
# recompile all 13 files per target. These are pulled OUT of
# SCRIBA_APP_WEBENGINE_SOURCES (above) so each file compiles exactly once.
# Deliberately no scriba_core/scriba_spell on the link: those exist only when
# BUILD_TESTS is on, and the Release scriba resolves Editor/prefs's spell/core
# symbols from its own direct objects. Consumers link scriba_core/scriba_spell/
# scriba_resources themselves as needed — object-lib FILES do not propagate.
add_library(scriba_editor OBJECT ${SCRIBA_EDITOR_SOURCES})
target_include_directories(scriba_editor PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(scriba_editor PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg stoppard)

add_library(scriba_prefs OBJECT ${SCRIBA_PREFS_SOURCES})
target_include_directories(scriba_prefs PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(scriba_prefs PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg stoppard)

# scriba links scriba_app for the shared app sources and compiles the rest
# (markdown core, spell/validation, typography, parsers) directly — these are
# exactly the files NOT in SCRIBA_APP_WEBENGINE_SOURCES, so nothing is compiled
# twice. It also links scriba_twemoji directly (the app renders emoji).
add_executable(scriba WIN32
    src/main.cpp
    src/preview/MarkdownParser.cpp
    src/validation/MdLintRules.cpp
    src/validation/MdLintConfig.cpp
    src/validation/MdLintEngine.cpp
    src/preview/MdRenderer.cpp
    src/preview/Typography.cpp
    src/css/CssUtils.cpp
    src/charts/EChartsParser.cpp
    src/charts/MermaidParser.cpp
    src/StaticHelpers.cpp
    src/editor/MdTable.cpp
    src/preview/Readability.h
    src/preview/Readability.cpp
    src/preview/JsSnippets.cpp
    src/io/CsvReader.cpp
    src/preview/PrintOptions.cpp
    src/spell/SpellChecker.cpp
    src/spell/SpellHighlighter.cpp
    src/validation/LinkValidator.cpp
    src/spell/GrammarChecker.cpp
    src/spell/StoppardEngine.cpp
    vendor/md4c/src/md4c.c
    vendor/md4c/src/md4c-html.c
    vendor/md4c/src/entity.c
)

target_include_directories(scriba PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/md4c/src
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scriba PRIVATE
    scriba_app
    scriba_editor
    scriba_prefs
    scriba_resources
    scriba_twemoji
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
        src/preview/MarkdownParser.cpp
        src/preview/MdRenderer.cpp
        src/preview/Typography.cpp
        src/css/CssUtils.cpp
        src/StaticHelpers.cpp
        src/editor/MdTable.cpp
        src/preview/Readability.cpp
        src/preview/JsSnippets.cpp
        src/io/CsvReader.cpp
        src/charts/EChartsParser.cpp
        src/charts/MermaidParser.cpp
        src/preview/PrintOptions.cpp
        vendor/md4c/src/md4c.c
        vendor/md4c/src/entity.c
    )
    target_include_directories(scriba_core PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/md4c/src
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(scriba_core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets)

    add_library(scriba_spell STATIC
        src/spell/SpellChecker.cpp
        src/spell/SpellHighlighter.cpp
        src/validation/LinkValidator.cpp
        src/validation/MdLintRules.cpp
        src/validation/MdLintConfig.cpp
        src/validation/MdLintEngine.cpp
        src/spell/GrammarChecker.cpp
        src/spell/StoppardEngine.cpp
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

    add_executable(test_print_options tests/test_print_options.cpp)
    target_link_libraries(test_print_options PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_print_options COMMAND ${_TEST_PREFIX} ./test_print_options)

    add_executable(test_md_renderer_helpers tests/test_md_renderer_helpers.cpp)
    target_link_libraries(test_md_renderer_helpers PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_md_renderer_helpers COMMAND ${_TEST_PREFIX} ./test_md_renderer_helpers)

    add_executable(test_chartsource tests/test_chartsource.cpp)
    target_link_libraries(test_chartsource PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_chartsource COMMAND ${_TEST_PREFIX} ./test_chartsource)

    add_executable(test_chartsource_docs tests/test_chartsource_docs.cpp)
    target_link_libraries(test_chartsource_docs PRIVATE scriba_core GTest::gtest_main)
    target_compile_definitions(test_chartsource_docs
        PRIVATE SCRIBA_DOCS_DIR="${CMAKE_SOURCE_DIR}/docs")
    add_test(NAME test_chartsource_docs COMMAND ${_TEST_PREFIX} ./test_chartsource_docs)

    add_executable(test_typography tests/test_typography.cpp)
    target_link_libraries(test_typography PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_typography COMMAND ${_TEST_PREFIX} ./test_typography)

    add_executable(test_css_utils tests/test_css_utils.cpp)
    target_link_libraries(test_css_utils PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_css_utils COMMAND ${_TEST_PREFIX} ./test_css_utils)

    add_executable(test_css_highlighter tests/test_css_highlighter.cpp src/css/CssHighlighter.cpp)
    target_include_directories(test_css_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_highlighter PRIVATE scriba_test_main)
    add_test(NAME test_css_highlighter COMMAND ${_TEST_PREFIX} ./test_css_highlighter)

    add_executable(test_corpus tests/test_corpus.cpp src/corpus/Corpus.cpp)
    target_include_directories(test_corpus PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_corpus PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_corpus COMMAND ${_TEST_PREFIX} ./test_corpus)

    add_executable(test_corpus_watcher tests/test_corpus_watcher.cpp src/corpus/CorpusWatcher.cpp)
    target_include_directories(test_corpus_watcher PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_corpus_watcher PRIVATE scriba_core scriba_test_main Qt6::Test)
    add_test(NAME test_corpus_watcher COMMAND ${_TEST_PREFIX} ./test_corpus_watcher)

    add_executable(test_corpus_files_panel tests/test_corpus_files_panel.cpp
        src/corpus/CorpusFilesPanel.cpp)
    target_include_directories(test_corpus_files_panel PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_corpus_files_panel PRIVATE scriba_core scriba_test_main Qt6::Test)
    add_test(NAME test_corpus_files_panel COMMAND ${_TEST_PREFIX} ./test_corpus_files_panel)

    add_executable(test_corpus_linkfix tests/test_corpus_linkfix.cpp src/corpus/LinkFixer.cpp)
    target_include_directories(test_corpus_linkfix PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_corpus_linkfix PRIVATE scriba_core scriba_test_main)
    add_test(NAME test_corpus_linkfix COMMAND ${_TEST_PREFIX} ./test_corpus_linkfix)

    add_executable(test_corpus_index tests/test_corpus_index.cpp src/corpus/CorpusIndex.cpp src/corpus/Corpus.cpp)
    target_include_directories(test_corpus_index PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_corpus_index PRIVATE scriba_core scriba_spell scriba_test_main)
    add_test(NAME test_corpus_index COMMAND ${_TEST_PREFIX} ./test_corpus_index)

    add_executable(test_static_helpers tests/test_static_helpers.cpp)
    target_link_libraries(test_static_helpers PRIVATE scriba_core GTest::gtest_main scriba_resources scriba_twemoji)
    add_test(NAME test_static_helpers COMMAND ${_TEST_PREFIX} ./test_static_helpers)

    add_executable(test_md_table tests/test_md_table.cpp)
    target_include_directories(test_md_table PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_md_table PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_md_table COMMAND ${_TEST_PREFIX} ./test_md_table)

    add_executable(test_spell_checker tests/test_spell_checker.cpp)
    target_link_libraries(test_spell_checker PRIVATE GTest::gtest scriba_spell scriba_resources)
    add_test(NAME test_spell_checker COMMAND ${_TEST_PREFIX} ./test_spell_checker)

    add_executable(test_spell_highlighter tests/test_spell_highlighter.cpp)
    target_include_directories(test_spell_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_spell_highlighter PRIVATE GTest::gtest scriba_spell Qt6::Test scriba_resources)
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

    add_executable(test_md_lint_structure tests/test_md_lint_structure.cpp)
    target_include_directories(test_md_lint_structure PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_md_lint_structure PRIVATE scriba_core GTest::gtest_main scriba_spell)
    add_test(NAME test_md_lint_structure COMMAND ${_TEST_PREFIX} ./test_md_lint_structure)

    add_executable(test_md_lint_whitespace tests/test_md_lint_whitespace.cpp)
    target_include_directories(test_md_lint_whitespace PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_md_lint_whitespace PRIVATE scriba_core GTest::gtest_main scriba_spell)
    add_test(NAME test_md_lint_whitespace COMMAND ${_TEST_PREFIX} ./test_md_lint_whitespace)

    add_executable(test_md_lint_links tests/test_md_lint_links.cpp)
    target_include_directories(test_md_lint_links PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_md_lint_links PRIVATE scriba_core GTest::gtest_main scriba_spell)
    add_test(NAME test_md_lint_links COMMAND ${_TEST_PREFIX} ./test_md_lint_links)

    add_executable(test_md_lint_config tests/test_md_lint_config.cpp)
    target_include_directories(test_md_lint_config PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_md_lint_config PRIVATE scriba_core GTest::gtest_main scriba_spell)
    add_test(NAME test_md_lint_config COMMAND ${_TEST_PREFIX} ./test_md_lint_config)

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
        tests/GitTestRepo.cpp
        ${SCRIBA_MERMAID_SOURCES}
        src/io/CsvColumnMapDialog.cpp
        src/preview/Preview.cpp)
    target_include_directories(test_mermaid_dialog PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(test_mermaid_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets
                Qt6::Test scriba_gitgraph)
    add_test(NAME test_mermaid_dialog
        COMMAND ${_TEST_PREFIX} ./test_mermaid_dialog)

    add_executable(test_gitgraph tests/test_gitgraph.cpp tests/GitTestRepo.cpp)
    target_include_directories(test_gitgraph PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(test_gitgraph
        PRIVATE scriba_gitgraph scriba_core GTest::gtest_main Qt6::Core)
    add_test(NAME test_gitgraph COMMAND ./test_gitgraph)

    add_executable(test_chart_dialog
        tests/test_chart_dialog.cpp
        src/charts/ChartDialog.cpp
        src/preview/Preview.cpp)
    target_link_libraries(test_chart_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_chart_dialog COMMAND ${_TEST_PREFIX} ./test_chart_dialog)

    add_executable(test_advanced_chart_dialog
        tests/test_advanced_chart_dialog.cpp
        src/charts/AdvancedChartDialog.cpp
        src/charts/ChartDialog.cpp
        src/preview/Preview.cpp)
    target_link_libraries(test_advanced_chart_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_advanced_chart_dialog COMMAND ${_TEST_PREFIX} ./test_advanced_chart_dialog)

    add_executable(test_stock_chart_dialog
        tests/test_stock_chart_dialog.cpp
        src/charts/StockChartDialog.cpp
        src/preview/Preview.cpp)
    target_link_libraries(test_stock_chart_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_stock_chart_dialog COMMAND ${_TEST_PREFIX} ./test_stock_chart_dialog)

    add_executable(test_katex_helper_dialog
        tests/test_katex_helper_dialog.cpp
        src/dialogs/KatexHelperDialog.cpp
        src/preview/Preview.cpp)
    target_link_libraries(test_katex_helper_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_katex_helper_dialog COMMAND ${_TEST_PREFIX} ./test_katex_helper_dialog)

    add_executable(test_mchem_helper_dialog
        tests/test_mchem_helper_dialog.cpp
        src/dialogs/MchemHelperDialog.cpp
        src/preview/Preview.cpp)
    target_link_libraries(test_mchem_helper_dialog
        PRIVATE scriba_core GTest::gtest_main Qt6::Widgets Qt6::WebEngineWidgets)
    add_test(NAME test_mchem_helper_dialog COMMAND ${_TEST_PREFIX} ./test_mchem_helper_dialog)

    add_executable(test_editor_autocomplete tests/test_editor_autocomplete.cpp)
    target_link_libraries(test_editor_autocomplete PRIVATE scriba_core GTest::gtest_main)
    add_test(NAME test_editor_autocomplete COMMAND ${_TEST_PREFIX} ./test_editor_autocomplete)

    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_editor_completion_ui tests/test_editor_completion_ui.cpp)
    target_include_directories(test_editor_completion_ui PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_completion_ui PRIVATE GTest::gtest test_editor_helpers Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_editor scriba_core scriba_spell scriba_resources scriba_twemoji)
    add_test(NAME test_editor_completion_ui COMMAND ${_TEST_PREFIX} ./test_editor_completion_ui)

    add_executable(test_folding tests/test_folding.cpp)
    target_include_directories(test_folding PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_folding PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_editor scriba_core scriba_spell scriba_resources)
    add_test(NAME test_folding COMMAND ${_TEST_PREFIX} ./test_folding)

    add_executable(test_gutter_line_height tests/test_gutter_line_height.cpp)
    target_include_directories(test_gutter_line_height PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_gutter_line_height PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_editor scriba_core scriba_spell scriba_resources)
    add_test(NAME test_gutter_line_height COMMAND ${_TEST_PREFIX} ./test_gutter_line_height)

    add_executable(test_gutter_width tests/test_gutter_width.cpp)
    target_include_directories(test_gutter_width PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_gutter_width PRIVATE GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test scriba_editor scriba_core scriba_spell scriba_resources)
    add_test(NAME test_gutter_width COMMAND ${_TEST_PREFIX} ./test_gutter_width)

    add_executable(test_editor_typing tests/test_editor_typing.cpp)
    target_include_directories(test_editor_typing PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_typing PRIVATE GTest::gtest test_editor_helpers scriba_editor scriba_spell scriba_resources)
    add_test(NAME test_editor_typing COMMAND ${_TEST_PREFIX} ./test_editor_typing)

    add_executable(test_editor_typing_2 tests/test_editor_typing_2.cpp)
    target_include_directories(test_editor_typing_2 PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_typing_2 PRIVATE GTest::gtest test_editor_helpers scriba_editor scriba_spell scriba_resources)
    add_test(NAME test_editor_typing_2 COMMAND ${_TEST_PREFIX} ./test_editor_typing_2)

    add_executable(test_editor_autocorrect tests/test_editor_autocorrect.cpp)
    target_include_directories(test_editor_autocorrect PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_autocorrect PRIVATE GTest::gtest test_editor_helpers scriba_editor scriba_spell scriba_resources)
    add_test(NAME test_editor_autocorrect COMMAND ${_TEST_PREFIX} ./test_editor_autocorrect)

    add_executable(test_editor_wrap tests/test_editor_wrap.cpp)
    target_include_directories(test_editor_wrap PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_wrap PRIVATE GTest::gtest test_editor_helpers scriba_editor scriba_spell scriba_resources)
    add_test(NAME test_editor_wrap COMMAND ${_TEST_PREFIX} ./test_editor_wrap)

    add_executable(test_enter_scroll tests/test_enter_scroll.cpp)
    target_include_directories(test_enter_scroll PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_enter_scroll PRIVATE GTest::gtest test_editor_helpers scriba_editor scriba_spell scriba_resources)
    add_test(NAME test_enter_scroll COMMAND ${_TEST_PREFIX} ./test_enter_scroll)

    add_executable(test_underline_overlay tests/test_underline_overlay.cpp)
    target_include_directories(test_underline_overlay PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_underline_overlay PRIVATE GTest::gtest scriba_editor scriba_core scriba_spell scriba_resources)
    add_test(NAME test_underline_overlay COMMAND ${_TEST_PREFIX} ./test_underline_overlay)

    add_executable(test_editor_scrollbar tests/test_editor_scrollbar.cpp)
    target_include_directories(test_editor_scrollbar PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_editor_scrollbar PRIVATE
        GTest::gtest Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Test
        scriba_editor scriba_core scriba_spell scriba_resources)
    add_test(NAME test_editor_scrollbar COMMAND ${_TEST_PREFIX} ./test_editor_scrollbar)

    add_executable(test_issue_summary tests/test_issue_summary.cpp)
    target_include_directories(test_issue_summary PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_issue_summary PRIVATE
        GTest::gtest scriba_editor scriba_core scriba_spell Qt6::Test scriba_resources)
    add_test(NAME test_issue_summary COMMAND ${_TEST_PREFIX} ./test_issue_summary)

    add_executable(test_spell_check_dialog tests/test_spell_check_dialog.cpp
        src/spell/SpellCheckDialog.cpp)
    target_include_directories(test_spell_check_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_spell_check_dialog PRIVATE
        GTest::gtest scriba_editor scriba_core scriba_spell Qt6::Test Qt6::Widgets scriba_resources)
    add_test(NAME test_spell_check_dialog COMMAND ${_TEST_PREFIX} ./test_spell_check_dialog)

    add_executable(test_preferences_search tests/test_preferences_search.cpp)
    target_include_directories(test_preferences_search PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_preferences_search PRIVATE
        scriba_editor scriba_prefs scriba_core scriba_spell scriba_app scriba_test_main Qt6::Test scriba_resources)
    add_test(NAME test_preferences_search COMMAND ${_TEST_PREFIX} ./test_preferences_search)

    add_executable(test_scroll_sync tests/test_scroll_sync.cpp)
    target_link_libraries(test_scroll_sync PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_scroll_sync COMMAND ${_TEST_PREFIX} ./test_scroll_sync)

    add_executable(test_scroll_sync_integration tests/test_scroll_sync_integration.cpp)
    target_link_libraries(test_scroll_sync_integration PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_scroll_sync_integration COMMAND ${_TEST_PREFIX} ./test_scroll_sync_integration)

    add_executable(test_html_to_markdown
        tests/test_html_to_markdown.cpp
        src/io/HtmlToMarkdown.cpp)
    target_include_directories(test_html_to_markdown PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_html_to_markdown
        PRIVATE GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets scriba_core scriba_resources)
    add_test(NAME test_html_to_markdown COMMAND ${_TEST_PREFIX} ./test_html_to_markdown)

    add_executable(test_anchor_navigation tests/test_anchor_navigation.cpp)
    target_link_libraries(test_anchor_navigation PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_anchor_navigation COMMAND ${_TEST_PREFIX} ./test_anchor_navigation)

    add_executable(test_issue_summary_mainwindow tests/test_issue_summary_mainwindow.cpp)
    target_link_libraries(test_issue_summary_mainwindow PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_issue_summary_mainwindow COMMAND ${_TEST_PREFIX} ./test_issue_summary_mainwindow)

    add_executable(test_emoji_preview tests/test_emoji_preview.cpp)
    target_link_libraries(test_emoji_preview PRIVATE
        scriba_core GTest::gtest test_editor_helpers Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources scriba_twemoji)
    add_test(NAME test_emoji_preview COMMAND ${_TEST_PREFIX} ./test_emoji_preview)

    add_executable(test_svg_upscaling tests/test_svg_upscaling.cpp)
    target_link_libraries(test_svg_upscaling PRIVATE
        scriba_core GTest::gtest test_editor_helpers Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources scriba_twemoji)
    add_test(NAME test_svg_upscaling COMMAND ${_TEST_PREFIX} ./test_svg_upscaling)

    add_executable(test_auto_save tests/test_auto_save.cpp)
    target_link_libraries(test_auto_save PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_auto_save COMMAND ${_TEST_PREFIX} ./test_auto_save)

    add_executable(test_corpus_unsaved tests/test_corpus_unsaved.cpp)
    target_link_libraries(test_corpus_unsaved PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_corpus_unsaved COMMAND ${_TEST_PREFIX} ./test_corpus_unsaved)

    add_executable(test_multi_tab_typing tests/test_multi_tab_typing.cpp)
    target_link_libraries(test_multi_tab_typing PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_multi_tab_typing COMMAND ${_TEST_PREFIX} ./test_multi_tab_typing)

    add_executable(test_corpus_recent tests/test_corpus_recent.cpp)
    target_link_libraries(test_corpus_recent PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_corpus_recent COMMAND ${_TEST_PREFIX} ./test_corpus_recent)

    add_executable(test_dirty_on_load tests/test_dirty_on_load.cpp)
    target_link_libraries(test_dirty_on_load PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_dirty_on_load COMMAND ${_TEST_PREFIX} ./test_dirty_on_load)

    add_executable(test_corpus_watcher_integration tests/test_corpus_watcher_integration.cpp)
    target_link_libraries(test_corpus_watcher_integration PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_corpus_watcher_integration COMMAND ${_TEST_PREFIX} ./test_corpus_watcher_integration)

    add_executable(test_corpus_export tests/test_corpus_export.cpp)
    target_link_libraries(test_corpus_export PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_corpus_export COMMAND ${_TEST_PREFIX} ./test_corpus_export)

    add_executable(test_corpus_restore tests/test_corpus_restore.cpp)
    target_link_libraries(test_corpus_restore PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_corpus_restore COMMAND ${_TEST_PREFIX} ./test_corpus_restore)

    add_executable(test_editor_initial_css tests/test_editor_initial_css.cpp)
target_link_libraries(test_editor_initial_css PRIVATE
    scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
add_test(NAME test_editor_initial_css COMMAND ${_TEST_PREFIX} ./test_editor_initial_css)

add_executable(test_find_dialog tests/test_find_dialog.cpp)
    target_link_libraries(test_find_dialog PRIVATE
        scriba_core GTest::gtest Qt6::Test scriba_spell scriba_app scriba_editor scriba_prefs scriba_resources)
    add_test(NAME test_find_dialog COMMAND ${_TEST_PREFIX} ./test_find_dialog)

    add_executable(test_print_css_file tests/test_print_css_file.cpp)
    target_compile_definitions(test_print_css_file PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(test_print_css_file PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_print_css_file COMMAND ${_TEST_PREFIX} ./test_print_css_file)

    add_executable(test_css_loader tests/test_css_loader.cpp
        src/css/CssLoader.cpp src/css/CssConfig.cpp)
    target_include_directories(test_css_loader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_loader PRIVATE scriba_core scriba_test_main Qt6::Core scriba_resources)
    add_test(NAME test_css_loader COMMAND ${_TEST_PREFIX} ./test_css_loader)

    add_executable(test_settings_migration tests/test_settings_migration.cpp)
    target_include_directories(test_settings_migration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_settings_migration PRIVATE scriba_core scriba_test_main Qt6::Core)
    add_test(NAME test_settings_migration COMMAND ${_TEST_PREFIX} ./test_settings_migration)

    add_executable(test_css_config tests/test_css_config.cpp src/css/CssConfig.cpp)
    target_include_directories(test_css_config PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_css_config PRIVATE scriba_core scriba_test_main Qt6::Core)
    add_test(NAME test_css_config COMMAND ${_TEST_PREFIX} ./test_css_config)

    add_executable(test_pdf_export tests/test_pdf_export.cpp
        src/io/ExportPdfDialog.cpp src/preview/JsRenderEngine.cpp src/preview/Preview.cpp
        src/css/CssConfig.cpp src/css/CssLoader.cpp src/css/CssEditorDialog.cpp src/css/CssHighlighter.cpp
        src/preview/PrintOptions.cpp)
    target_include_directories(test_pdf_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_pdf_export PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::PrintSupport Qt6::Test scriba_resources)
    add_test(NAME test_pdf_export COMMAND ${_TEST_PREFIX} ./test_pdf_export)

    add_executable(test_preview_pagination tests/test_preview_pagination.cpp
        src/preview/PreviewPagination.cpp)
    target_include_directories(test_preview_pagination PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_preview_pagination PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Test)
    add_test(NAME test_preview_pagination COMMAND ${_TEST_PREFIX} ./test_preview_pagination)

    add_executable(test_export_html tests/test_export_html.cpp
        src/io/ExportHtmlDialog.cpp src/preview/JsRenderEngine.cpp src/preview/Preview.cpp
        src/css/CssConfig.cpp src/css/CssLoader.cpp)
    target_include_directories(test_export_html PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_export_html PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Network Qt6::Test scriba_resources)
    add_test(NAME test_export_html COMMAND ${_TEST_PREFIX} ./test_export_html)

    add_executable(test_docx_export tests/test_docx_export.cpp
    src/io/HtmlToOoxml.cpp
        src/io/DocxExporter.cpp
        src/io/ZipReader.cpp
        src/css/UnitConverter.cpp
        src/css/CssValueParser.cpp)
    target_include_directories(test_docx_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_docx_export PRIVATE
        scriba_core GTest::gtest Qt6::Gui Qt6::Svg mathml2omml miniz)
    add_test(NAME test_docx_export COMMAND ${_TEST_PREFIX} ./test_docx_export)

    add_executable(test_docx_image_export tests/test_docx_image_export.cpp
        src/preview/JsRenderEngine.cpp src/preview/Preview.cpp
        src/css/CssConfig.cpp src/css/CssLoader.cpp src/io/HtmlToOoxml.cpp
        src/css/UnitConverter.cpp
        src/css/CssValueParser.cpp)
    target_include_directories(test_docx_image_export PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_docx_image_export PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::Network Qt6::Svg mathml2omml scriba_resources)
    add_test(NAME test_docx_image_export COMMAND ${_TEST_PREFIX} ./test_docx_image_export)

    add_executable(test_mchem_integration tests/test_mchem_integration.cpp)
    target_compile_definitions(test_mchem_integration PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources")
    target_link_libraries(test_mchem_integration PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_mchem_integration COMMAND ${_TEST_PREFIX} ./test_mchem_integration)

    add_executable(test_csv_reader tests/test_csv_reader.cpp src/io/CsvReader.cpp)
    target_include_directories(test_csv_reader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_csv_reader PRIVATE GTest::gtest_main Qt6::Core)
    add_test(NAME test_csv_reader COMMAND ${_TEST_PREFIX} ./test_csv_reader)

    add_executable(test_zip_reader tests/test_zip_reader.cpp src/io/ZipReader.cpp)
    target_include_directories(test_zip_reader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_zip_reader PRIVATE GTest::gtest_main Qt6::Core miniz)
    add_test(NAME test_zip_reader COMMAND ${_TEST_PREFIX} ./test_zip_reader)

    add_executable(test_ooxml_to_html tests/test_ooxml_to_html.cpp
        src/io/OoxmlToHtml.cpp
    src/io/OoxmlConverter.cpp)
    target_include_directories(test_ooxml_to_html PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_ooxml_to_html PRIVATE
        GTest::gtest_main Qt6::Core Qt6::Gui mathml2omml)
    add_test(NAME test_ooxml_to_html COMMAND ${_TEST_PREFIX} ./test_ooxml_to_html)

    add_executable(test_docx_import tests/test_docx_import.cpp
        src/io/DocxImporter.cpp src/io/ZipReader.cpp src/io/OoxmlToHtml.cpp
    src/io/OoxmlConverter.cpp
        src/io/HtmlToMarkdown.cpp src/io/HtmlToOoxml.cpp
        src/io/DocxExporter.cpp src/css/UnitConverter.cpp src/css/CssValueParser.cpp)
    target_include_directories(test_docx_import PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_docx_import PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets Qt6::WebChannel Qt6::Network Qt6::Svg mathml2omml miniz scriba_resources)
    add_test(NAME test_docx_import COMMAND ${_TEST_PREFIX} ./test_docx_import)

    add_executable(test_pdf_import tests/test_pdf_import.cpp
        src/io/PdfImporter.cpp)
    target_include_directories(test_pdf_import PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_pdf_import PRIVATE
        scriba_core GTest::gtest Qt6::Widgets Qt6::WebEngineWidgets scriba_resources)
    add_test(NAME test_pdf_import COMMAND ${_TEST_PREFIX} ./test_pdf_import)

    endif()

