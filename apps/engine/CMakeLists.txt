# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(zen1-engine
    main.cpp
    controls.cpp
    cps1_selftest.cpp
    library_scan.cpp
    presenter.cpp
    test_card.cpp
    tiny_font.cpp
    ipc_client.cpp)
target_link_libraries(zen1-engine PRIVATE zen1::common zen1::input zen1::emu zen1_cps1 SDL3::SDL3)
install(TARGETS zen1-engine RUNTIME DESTINATION bin)
