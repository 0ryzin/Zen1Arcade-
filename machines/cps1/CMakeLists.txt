# SPDX-License-Identifier: GPL-3.0-or-later
# Capcom CPS1 machine module (also provides the engine's game registry).
add_library(zen1_cps1 STATIC cps1.cpp cps1_video.cpp cps1_games.cpp oki6295.cpp)
target_include_directories(zen1_cps1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_cps1 PUBLIC zen1::emu PRIVATE zen1_moira zen1_z80 zen1_ymfm)
add_library(zen1::cps1 ALIAS zen1_cps1)
