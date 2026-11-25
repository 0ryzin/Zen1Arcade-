# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(zen1_tests
    main.cpp
    test_display_plan.cpp
    test_settings.cpp
    test_data_root.cpp
    test_assignment.cpp
    test_edid.cpp
    test_cps1.cpp
    test_pacman.cpp
    test_galaxian.cpp
    test_neogeo.cpp
    test_gesture.cpp
    test_cps2.cpp)
target_include_directories(zen1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_tests PRIVATE zen1::common zen1::input zen1::display zen1::machines)
add_test(NAME zen1_tests COMMAND zen1_tests)
