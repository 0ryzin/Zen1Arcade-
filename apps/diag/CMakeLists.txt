# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(zen1-diag main.cpp)
target_link_libraries(zen1-diag PRIVATE zen1::common zen1::input zen1::display)
if(ZEN1_STATIC_DIAG)
    # Fully static so the same file runs on any Linux (incl. SteamOS)
    # regardless of its glibc version. It uses no NSS/DNS functions.
    target_link_options(zen1-diag PRIVATE -static)
endif()
install(TARGETS zen1-diag RUNTIME DESTINATION bin)
