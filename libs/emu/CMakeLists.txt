# SPDX-License-Identifier: GPL-3.0-or-later
find_package(ZLIB REQUIRED)
add_library(zen1_emu STATIC src/zip.cpp src/roms.cpp)
target_include_directories(zen1_emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(zen1_emu PUBLIC zen1::common ZLIB::ZLIB)
add_library(zen1::emu ALIAS zen1_emu)
