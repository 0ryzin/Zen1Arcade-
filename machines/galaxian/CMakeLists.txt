# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_galaxian STATIC galaxian.cpp)
target_include_directories(zen1_galaxian PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_galaxian PUBLIC zen1::emu PRIVATE zen1_z80)
