# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(zen1_tests
    main.cpp
    test_display_plan.cpp
    test_settings.cpp
    test_data_root.cpp
    test_assignment.cpp
    test_edid.cpp)
target_include_directories(zen1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zen1_tests PRIVATE zen1::common zen1::input zen1::display)
add_test(NAME zen1_tests COMMAND zen1_tests)
