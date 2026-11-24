# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_display STATIC src/outputs_linux.cpp src/edid.cpp)
target_include_directories(zen1_display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(zen1_display PUBLIC zen1::common)
add_library(zen1::display ALIAS zen1_display)
