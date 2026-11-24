# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_common STATIC
    src/data_root.cpp
    src/settings.cpp
    src/display_plan.cpp
    src/log.cpp)
target_include_directories(zen1_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated)
add_library(zen1::common ALIAS zen1_common)
