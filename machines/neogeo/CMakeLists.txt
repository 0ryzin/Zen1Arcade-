# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_neogeo STATIC neogeo.cpp)
target_include_directories(zen1_neogeo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_neogeo PUBLIC zen1::emu PRIVATE zen1_moira zen1_z80 zen1_ymfm)
