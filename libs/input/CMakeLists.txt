# SPDX-License-Identifier: GPL-3.0-or-later
add_library(zen1_input STATIC
    src/devices_linux.cpp
    src/assignment.cpp
    src/bindings.cpp
    src/evdev_panel.cpp
    src/calibration.cpp
    src/mapping.cpp
    src/gesture.cpp)
target_include_directories(zen1_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(zen1_input PUBLIC zen1::common)
add_library(zen1::input ALIAS zen1_input)
