# SPDX-License-Identifier: GPL-3.0-or-later
# Vendored components. Each folder keeps its original LICENSE; see THIRD_PARTY.md.
# Warnings from third-party code are not ours to fix, so they are silenced here.

# Moira: Motorola 68000 family CPU (MIT)
add_library(zen1_moira STATIC
    moira/Moira.cpp
    moira/MoiraCore68000.cpp
    moira/MoiraCore68010.cpp
    moira/MoiraCore68020.cpp
    moira/MoiraDebugger.cpp
    moira/Cache/Cache.cpp
    moira/FPU/MFloat80.cpp
    moira/FPU/FpuEnv.cpp
    moira/FPU/FpuEnvMath.cpp
    moira/FPU/FpuFormats.cpp
    moira/FPU/FPU.cpp
    moira/MMU/MMU030.cpp
    moira/MMU/MMU040.cpp)
target_include_directories(zen1_moira SYSTEM PUBLIC moira)
target_compile_options(zen1_moira PRIVATE -w)

# superzazu z80: Zilog Z80 CPU (MIT)
add_library(zen1_z80 STATIC z80/z80.c)
target_include_directories(zen1_z80 SYSTEM PUBLIC z80)
target_compile_options(zen1_z80 PRIVATE -w)

# ymfm: Yamaha FM sound chips, YM2151 used (BSD-3-Clause)
add_library(zen1_ymfm STATIC ymfm/ymfm_opm.cpp)
target_include_directories(zen1_ymfm SYSTEM PUBLIC ymfm)
target_compile_options(zen1_ymfm PRIVATE -w)
