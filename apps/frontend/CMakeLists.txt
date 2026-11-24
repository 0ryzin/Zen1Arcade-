# SPDX-License-Identifier: GPL-3.0-or-later
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(zen1-arcade
    main.cpp
    backend.hpp backend.cpp
    input_watcher.hpp input_watcher.cpp
    qml.qrc)

target_link_libraries(zen1-arcade PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    zen1::common zen1::input zen1::display)

install(TARGETS zen1-arcade RUNTIME DESTINATION bin)
