# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory(cps1)
add_subdirectory(pacman)
add_subdirectory(galaxian)
add_subdirectory(neogeo)
add_subdirectory(c1942)

# All machine modules behind the engine-wide game registry.
add_library(zen1_machines STATIC registry.cpp)
target_link_libraries(zen1_machines PUBLIC zen1::emu zen1_cps1 zen1_pacman zen1_galaxian zen1_neogeo zen1_c1942)
add_library(zen1::machines ALIAS zen1_machines)
