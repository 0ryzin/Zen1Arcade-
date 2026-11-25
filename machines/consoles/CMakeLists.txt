# SPDX-License-Identifier: GPL-3.0-or-later
#
# Console machine modules. Each emulation core is built, together with its
# Zen1 wrapper, into one relocatable object whose internal symbols are then
# made local (objcopy keeps only zen1::consoles::make* global): the cores share class names
# (Memory, Video, Blip_Buffer...) and must not see each other. Only the
# wrapper's make...() function (ZEN1_CORE_API) stays visible. COMDAT groups
# are dropped too, so the linker never swaps a core's local copy of an
# inline or template function for another object's.
set(TP ${PROJECT_SOURCE_DIR}/third_party)
set(EMU_INC ${PROJECT_SOURCE_DIR}/libs/emu/include)

function(zen1_core target)
    cmake_parse_arguments(A "" "" "SOURCES;INCLUDES;DEFINES" ${ARGN})
    add_library(${target}_obj OBJECT ${A_SOURCES})
    target_include_directories(${target}_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${EMU_INC})
    target_include_directories(${target}_obj SYSTEM PRIVATE ${A_INCLUDES})
    target_compile_definitions(${target}_obj PRIVATE ${A_DEFINES})
    target_compile_options(${target}_obj PRIVATE -w -fvisibility=hidden -fvisibility-inlines-hidden -fno-gnu-unique -fno-strict-aliasing
                           $<$<CONFIG:Debug>:-O1>)
    set_target_properties(${target}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(merged ${CMAKE_CURRENT_BINARY_DIR}/${target}_merged.o)
    set(isolated ${CMAKE_CURRENT_BINARY_DIR}/${target}.o)
    add_custom_command(
        OUTPUT ${isolated}
        COMMAND ${CMAKE_LINKER} -r -o ${merged} "$<TARGET_OBJECTS:${target}_obj>"
        COMMAND ${CMAKE_OBJCOPY} --wildcard --keep-global-symbol=_ZN4zen18consoles*make* -R .group ${merged} ${isolated}
        DEPENDS ${target}_obj "$<TARGET_OBJECTS:${target}_obj>"
        COMMAND_EXPAND_LISTS
        COMMENT "Isolating ${target}")
    add_library(${target} STATIC ${isolated})
    set_source_files_properties(${isolated} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

# Game Boy / Game Boy Color: Gearboy (GPL-3.0)
file(GLOB GB_SRC ${TP}/gearboy/src/*.cpp ${TP}/gearboy/src/audio/*.cpp)
zen1_core(zen1_core_gb
    SOURCES gb.cpp ${GB_SRC} ${TP}/gearboy/deps/miniz/miniz.c
    INCLUDES ${TP}/gearboy/src ${TP}/gearboy/deps/miniz
    DEFINES MINIZ_NO_TIME)

# Master System / Game Gear / SG-1000: Gearsystem (GPL-3.0)
file(GLOB GS_SRC ${TP}/gearsystem/src/*.cpp ${TP}/gearsystem/src/audio/*.cpp)
zen1_core(zen1_core_sega8
    SOURCES sega8.cpp ${GS_SRC} ${TP}/gearsystem/src/audio/emu2413/emu2413.c ${TP}/gearsystem/deps/miniz/miniz.c
    INCLUDES ${TP}/gearsystem/src ${TP}/gearsystem/deps/miniz
    DEFINES MINIZ_NO_TIME)

# PC Engine: Geargrafx (GPL-3.0) and the libraries its media layer uses
file(GLOB GG_SRC ${TP}/geargrafx/src/*.cpp)
list(FILTER GG_SRC EXCLUDE REGEX "(cdrom_drive|cdrom_physical_image|media_file_libretro)")
set(GG_DEPS ${TP}/geargrafx/platforms/shared/dependencies)
zen1_core(zen1_core_pce
    SOURCES pce.cpp ${GG_SRC} ${GG_DEPS}/qon/qon.cpp ${GG_DEPS}/miniz/miniz.c ${GG_DEPS}/minivorbis/minivorbis.c
            ${GG_DEPS}/libchdr/src/libchdr_bitstream.c ${GG_DEPS}/libchdr/src/libchdr_cdrom.c
            ${GG_DEPS}/libchdr/src/libchdr_chd.c ${GG_DEPS}/libchdr/src/libchdr_flac.c
            ${GG_DEPS}/libchdr/src/libchdr_huffman.c ${GG_DEPS}/lzma/src/LzFind.c ${GG_DEPS}/lzma/src/LzmaEnc.c
            ${GG_DEPS}/lzma/src/LzmaDec.c ${GG_DEPS}/lzma/src/CpuArch.c
    INCLUDES ${TP}/geargrafx/src ${GG_DEPS}/miniz ${GG_DEPS}/qon ${GG_DEPS}/minivorbis ${GG_DEPS}/libchdr/include
             ${GG_DEPS}/lzma/include ${GG_DEPS}/zstd ${GG_DEPS}/json
    DEFINES MINIZ_NO_TIME GG_DISABLE_DISASSEMBLER GG_DISABLE_VGMRECORDER Z7_ST ZSTD_DISABLE_ASM)

# NES: Nes_Emu / QuickNES (LGPL-2.1-or-later)
file(GLOB NES_SRC ${TP}/quicknes/nes_emu/*.cpp)
zen1_core(zen1_core_nes
    SOURCES nes.cpp ${NES_SRC}
    INCLUDES ${TP}/quicknes/nes_emu ${TP}/quicknes)

# SNES: LakeSnes (MIT)
file(GLOB SNES_SRC ${TP}/lakesnes/snes/*.c)
zen1_core(zen1_core_snes
    SOURCES snes.cpp ${SNES_SRC}
    INCLUDES ${TP}/lakesnes/snes)

# Mega Drive / Genesis: clownmdemu (AGPL-3.0-or-later, see THIRD_PARTY.md)
set(CMD ${TP}/clownmdemu)
file(GLOB MD_SRC ${CMD}/source/*.c)
list(FILTER MD_SRC EXCLUDE REGEX "mega-cd-boot-rom\\.c$")
zen1_core(zen1_core_md
    SOURCES md.cpp ${MD_SRC} ${CMD}/libraries/clown68000/source/interpreter/clown68000.c
            ${CMD}/libraries/clown68000/source/common/opcode.c ${CMD}/libraries/clownz80/source/interpreter.c
            ${CMD}/libraries/clownz80/source/common.c
    INCLUDES ${CMD}/source)

# Atari 7800: ProSystem (GPL-2.0-or-later), with Zen1's stdio stand-in for
# the libretro-common file functions it uses
file(GLOB A78_SRC ${TP}/prosystem/core/*.c ${TP}/prosystem/bupboop/coretone/*.c)
zen1_core(zen1_core_a7800
    SOURCES a7800.cpp ${A78_SRC} ${TP}/prosystem/shim/file_stream.c
    INCLUDES ${TP}/prosystem/core ${TP}/prosystem/shim ${TP}/prosystem/bupboop/coretone ${TP}/prosystem)

# Game Boy Advance: mGBA (MPL-2.0), built by its own CMake project as a
# static library (no frontends, no optional libraries), then isolated with
# its Zen1 wrapper like the cores above.
include(ExternalProject)
set(MGBA_SRC ${TP}/mgba)
set(MGBA_BIN ${CMAKE_CURRENT_BINARY_DIR}/mgba-build)
ExternalProject_Add(mgba_ext
    SOURCE_DIR ${MGBA_SRC}
    BINARY_DIR ${MGBA_BIN}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DBUILD_QT=OFF -DBUILD_SDL=OFF
        -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_LIBRETRO=OFF -DBUILD_PERF=OFF -DBUILD_TEST=OFF
        -DBUILD_SUITE=OFF -DBUILD_CINEMA=OFF -DBUILD_EXAMPLE=OFF -DBUILD_HEADLESS=OFF -DBUILD_UPDATER=OFF
        -DBUILD_PYTHON=OFF -DUSE_FFMPEG=OFF -DUSE_ZLIB=OFF -DUSE_MINIZIP=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF
        -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_LZMA=OFF -DUSE_EPOXY=OFF -DUSE_DISCORD_RPC=OFF -DUSE_EDITLINE=OFF
        -DUSE_LUA=OFF -DUSE_JSON_C=OFF -DUSE_FREETYPE=OFF -DENABLE_SCRIPTING=OFF -DENABLE_DEBUGGERS=OFF
        -DENABLE_GDB_STUB=OFF -DM_CORE_GB=OFF -DDISABLE_FRONTENDS=ON -DBUILD_LTO=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${MGBA_BIN} --target mgba
    INSTALL_COMMAND ""
    BUILD_ALWAYS ON
    BUILD_BYPRODUCTS ${MGBA_BIN}/libmgba.a)
add_library(zen1_core_gba_obj OBJECT gba.cpp)
add_dependencies(zen1_core_gba_obj mgba_ext)
target_include_directories(zen1_core_gba_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${EMU_INC})
# The same feature switches mGBA is compiled with: they change its structures.
target_compile_definitions(zen1_core_gba_obj PRIVATE BUILD_STATIC ENABLE_DIRECTORIES ENABLE_VFS ENABLE_VFS_FD M_CORE_GBA USE_PTHREADS)
target_include_directories(zen1_core_gba_obj SYSTEM PRIVATE ${MGBA_BIN}/include ${MGBA_SRC}/include)
target_compile_options(zen1_core_gba_obj PRIVATE -w -fvisibility=hidden -fvisibility-inlines-hidden -fno-gnu-unique)
set_target_properties(zen1_core_gba_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba.o
    COMMAND ${CMAKE_LINKER} -r -o ${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba_merged.o "$<TARGET_OBJECTS:zen1_core_gba_obj>"
            --whole-archive ${MGBA_BIN}/libmgba.a --no-whole-archive
    COMMAND ${CMAKE_OBJCOPY} --wildcard --keep-global-symbol=_ZN4zen18consoles*make* -R .group
            ${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba_merged.o ${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba.o
    DEPENDS zen1_core_gba_obj "$<TARGET_OBJECTS:zen1_core_gba_obj>" mgba_ext ${MGBA_BIN}/libmgba.a
    COMMAND_EXPAND_LISTS
    COMMENT "Isolating zen1_core_gba")
add_library(zen1_core_gba STATIC ${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba.o)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/zen1_core_gba.o PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
set_target_properties(zen1_core_gba PROPERTIES LINKER_LANGUAGE CXX)
find_package(Threads REQUIRED)
target_link_libraries(zen1_core_gba INTERFACE Threads::Threads m)

add_library(zen1_consoles STATIC consoles.cpp)
target_include_directories(zen1_consoles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${PROJECT_SOURCE_DIR}/machines/ares)
target_link_libraries(zen1_consoles PUBLIC zen1::emu
    PRIVATE zen1_core_gb zen1_core_sega8 zen1_core_pce zen1_core_nes zen1_core_snes zen1_core_md zen1_core_gba zen1_core_a7800 zen1_ares)
