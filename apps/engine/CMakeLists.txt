# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(zen1-engine
    main.cpp
    test_card.cpp
    tiny_font.cpp
    ipc_client.cpp)
target_link_libraries(zen1-engine PRIVATE zen1::common zen1::input SDL3::SDL3)
install(TARGETS zen1-engine RUNTIME DESTINATION bin)
