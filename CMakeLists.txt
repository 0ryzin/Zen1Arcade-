# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)

project(Zen1Arcade
    VERSION 0.1.0
    DESCRIPTION "Zen1 Arcade - Linux arcade emulator platform"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(ZEN1_BUILD_FRONTEND "Build the Qt Quick frontend (needs Qt 6)" ON)
option(ZEN1_BUILD_ENGINE   "Build the engine runner (needs SDL3)"     ON)
option(ZEN1_BUILD_TESTS    "Build unit tests"                          ON)
option(ZEN1_STATIC_DIAG    "Link zen1-diag fully static (portable)"    OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

configure_file(libs/common/include/zen1/version.hpp.in
               ${CMAKE_BINARY_DIR}/generated/zen1/version.hpp @ONLY)

add_subdirectory(libs/common)
add_subdirectory(libs/input)
add_subdirectory(libs/display)
add_subdirectory(apps/diag)

if(ZEN1_BUILD_ENGINE)
    find_package(SDL3 CONFIG)
    if(SDL3_FOUND)
        add_subdirectory(apps/engine)
    else()
        message(WARNING "SDL3 not found: zen1-engine will not be built")
    endif()
endif()

if(ZEN1_BUILD_FRONTEND)
    find_package(Qt6 6.2 COMPONENTS Core Gui Qml Quick)
    if(Qt6_FOUND)
        add_subdirectory(apps/frontend)
    else()
        message(WARNING "Qt 6 not found: zen1-arcade frontend will not be built")
    endif()
endif()

if(ZEN1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
