cmake_minimum_required(VERSION 3.16)
project(oreon-system-manager VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ProcessRunner.cpp
    src/Settings.cpp
    src/pages/PackagesPage.cpp
    src/pages/ReposPage.cpp
    src/pages/ContainersPage.cpp
    src/pages/DriversPage.cpp
    src/widgets/OutputPanel.cpp
    src/widgets/Sidebar.cpp
    src/widgets/CommandPalette.cpp
    resources.qrc
)

set(HEADERS
    src/MainWindow.h
    src/ProcessRunner.h
    src/Settings.h
    src/pages/PackagesPage.h
    src/pages/ReposPage.h
    src/pages/ContainersPage.h
    src/pages/DriversPage.h
    src/widgets/OutputPanel.h
    src/widgets/Sidebar.h
    src/widgets/CommandPalette.h
)

qt_add_executable(oreon-system-manager ${SOURCES} ${HEADERS})

target_include_directories(oreon-system-manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oreon-system-manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
)

install(TARGETS oreon-system-manager RUNTIME DESTINATION bin)
install(FILES packaging/oreon-system-manager.desktop
        DESTINATION share/applications)
install(FILES assets/logo.png
        DESTINATION share/icons/hicolor/200x200/apps
        RENAME oreon-system-manager.png)
install(FILES packaging/org.oreon.SystemManager.policy
        DESTINATION share/polkit-1/actions)
