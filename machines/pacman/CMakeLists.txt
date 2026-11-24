# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_pacman STATIC pacman.cpp)
target_include_directories(zen1_pacman PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_pacman PUBLIC zen1::emu PRIVATE zen1_z80)
