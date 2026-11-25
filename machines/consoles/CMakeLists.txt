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

add_library(zen1_consoles STATIC consoles.cpp)
target_include_directories(zen1_consoles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${PROJECT_SOURCE_DIR}/machines/ares)
target_link_libraries(zen1_consoles PUBLIC zen1::emu
    PRIVATE zen1_core_gb zen1_core_sega8 zen1_core_pce zen1_core_nes zen1_core_snes zen1_core_md zen1_ares)
