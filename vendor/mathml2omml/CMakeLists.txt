cmake_minimum_required(VERSION 3.16)

project(mathml2omml_vendor CXX)

# Vendored mathml2omml v3.0.0 (https://github.com/lazyarse/mathml2omml).
# This copy is the working copy for local code changes and may diverge from
# upstream. Requires C++23 (inherited from the top-level CMAKE_CXX_STANDARD).

add_library(mathml2omml STATIC
    mathml2omml.cpp
)

target_include_directories(mathml2omml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
