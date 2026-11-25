# SPDX-License-Identifier: GPL-3.0-or-later
#
# The ares emulation cores (ISC licence, third_party/ares): Atari 2600, SNES
# with enhancement chips (Super FX, SA-1, DSP, Cx4, ...), Sega 32X and
# Mega CD, PC Engine CD, Nintendo 64. ares is built by its own CMake project
# (headless: no ares UI, no bundled firmware), then linked together with
# Zen1's host for it into one relocatable object whose symbols are made local
# except zen1::aresx::* - exactly as the other console cores are isolated
# (machines/consoles/CMakeLists.txt): ares's nall, ymfm and libchdr never meet
# the copies other parts of Zen1 use.
include(ExternalProject)
find_package(Threads REQUIRED)

set(ARES_SRC ${PROJECT_SOURCE_DIR}/third_party/ares)
set(ARES_BIN ${CMAKE_CURRENT_BINARY_DIR}/ares-build)
set(ARES_LIBS
    ${ARES_BIN}/ares/libares.a
    ${ARES_BIN}/mia/libmia.a
    ${ARES_BIN}/libco/liblibco.a
    ${ARES_BIN}/thirdparty/sljit.a
    ${ARES_BIN}/thirdparty/chdr-static.a
    ${ARES_BIN}/thirdparty/libchdr/deps/zstd-1.5.7/zstd.a
    ${ARES_BIN}/thirdparty/libchdr/deps/miniz-3.1.1/miniz.a
    ${ARES_BIN}/thirdparty/tzxfile.a
    ${ARES_BIN}/thirdparty/qon.a)
set(ARES_NALL_OBJS
    ${ARES_BIN}/nall/nall/CMakeFiles/nall.dir/nall.cpp.o
    ${ARES_BIN}/nall/nall/CMakeFiles/nall.dir/sljitAllocator.cpp.o)

ExternalProject_Add(ares_ext
    SOURCE_DIR ${ARES_SRC}
    BINARY_DIR ${ARES_BIN}
    LIST_SEPARATOR |
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_CXX_FLAGS=-fno-gnu-unique
        -DARES_HEADLESS=ON
        -DARES_CORES=a26|sfc|md|pce|n64
        -DARES_ENABLE_CHD=ON
        -DARES_BUILD_LOCAL=OFF
        -DARES_SKIP_DEPS=ON
        -DENABLE_IPO=OFF
        -DENABLE_CCACHE=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${ARES_BIN} --target ares mia nall libco
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${ARES_LIBS} ${ARES_NALL_OBJS})

set(ARES_DEFS ARES_ENABLE_CHD BUILD_RELEASE CORE_A26 CORE_SFC CORE_MD CORE_PCE CORE_N64 MIA_LIBRARY NDEBUG
    PROFILE_PERFORMANCE SLJIT_HAVE_CONFIG_POST=1 SLJIT_HAVE_CONFIG_PRE=1 VULKAN)
add_library(zen1_ares_host OBJECT ares_machine.cpp)
add_dependencies(zen1_ares_host ares_ext)
target_compile_definitions(zen1_ares_host PRIVATE ${ARES_DEFS})
target_include_directories(zen1_ares_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libs/emu/include
    ${PROJECT_SOURCE_DIR}/machines/consoles)
target_include_directories(zen1_ares_host SYSTEM PRIVATE ${ARES_BIN}/mia ${ARES_BIN}/ares
    ${ARES_BIN}/thirdparty/libchdr/include ${ARES_SRC} ${ARES_SRC}/ares ${ARES_SRC}/mia ${ARES_SRC}/thirdparty
    ${ARES_SRC}/thirdparty/libchdr/include ${ARES_SRC}/nall)
target_compile_options(zen1_ares_host PRIVATE -w -fno-char8_t -fno-strict-aliasing -fwrapv -fvisibility=hidden
    -fvisibility-inlines-hidden -fno-gnu-unique)
set_target_properties(zen1_ares_host PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(merged ${CMAKE_CURRENT_BINARY_DIR}/zen1_ares_merged.o)
set(isolated ${CMAKE_CURRENT_BINARY_DIR}/zen1_ares.o)
add_custom_command(
    OUTPUT ${isolated}
    COMMAND ${CMAKE_LINKER} -r -o ${merged} "$<TARGET_OBJECTS:zen1_ares_host>" ${ARES_NALL_OBJS}
            --whole-archive ${ARES_LIBS} --no-whole-archive
    COMMAND ${CMAKE_OBJCOPY} --wildcard --keep-global-symbol=_ZN4zen15aresx* -R .group ${merged} ${isolated}
    DEPENDS zen1_ares_host "$<TARGET_OBJECTS:zen1_ares_host>" ares_ext ${ARES_LIBS}
    COMMAND_EXPAND_LISTS
    COMMENT "Isolating the ares cores")
add_library(zen1_ares STATIC ${isolated})
set_source_files_properties(${isolated} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
set_target_properties(zen1_ares PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(zen1_ares INTERFACE ${CMAKE_DL_LIBS} Threads::Threads)

# ares's game database (mia) sits next to zen1-engine, where ares looks for it.
add_custom_target(zen1_ares_database ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARES_SRC}/mia/Database ${CMAKE_BINARY_DIR}/apps/engine/Database)
install(DIRECTORY ${ARES_SRC}/mia/Database DESTINATION bin)
