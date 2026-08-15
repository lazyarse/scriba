cmake_minimum_required(VERSION 3.16)

project(ta_lib_vendor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

file(GLOB TA_LIB_COMMON_SOURCES CONFIGURE_DEPENDS src/ta_common/*.c)
file(GLOB TA_LIB_FUNC_SOURCES CONFIGURE_DEPENDS src/ta_func/*.c)
file(GLOB_RECURSE TA_LIB_ABSTRACT_SOURCES CONFIGURE_DEPENDS src/ta_abstract/*.c)

add_library(ta-lib STATIC
    ${TA_LIB_COMMON_SOURCES}
    ${TA_LIB_FUNC_SOURCES}
    ${TA_LIB_ABSTRACT_SOURCES}
)

target_include_directories(ta-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(ta-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ta_func
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ta_common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ta_abstract
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ta_abstract/frames
)

if(MSVC)
    target_compile_options(ta-lib PRIVATE /W0)
else()
    target_compile_options(ta-lib PRIVATE -w)
endif()