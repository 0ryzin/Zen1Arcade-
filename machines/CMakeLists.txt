# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory(cps1)
add_subdirectory(pacman)

# All machine modules behind the engine-wide game registry.
add_library(zen1_machines STATIC registry.cpp)
target_link_libraries(zen1_machines PUBLIC zen1::emu zen1_cps1 zen1_pacman)
add_library(zen1::machines ALIAS zen1_machines)
