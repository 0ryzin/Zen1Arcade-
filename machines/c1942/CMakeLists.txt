# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_c1942 STATIC c1942.cpp)
target_include_directories(zen1_c1942 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_c1942 PUBLIC zen1::emu PRIVATE zen1_z80 zen1_ymfm)
