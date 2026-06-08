cmake_minimum_required(VERSION 3.16)
project(oreon-system-manager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Concurrent)

# All non-main code lives in a static lib so tests can link against it
# without re-compiling every source file.
add_library(oreon_core STATIC
    src/mainwindow.cpp
    src/widgets/sidebar.cpp
    src/pages/packagepage.cpp
    src/pages/repopage.cpp
    src/pages/containerpage.cpp
    src/pages/driverspage.cpp
)

target_link_libraries(oreon_core PUBLIC Qt6::Widgets Qt6::Core Qt6::Concurrent)
target_include_directories(oreon_core PUBLIC src)

qt_add_executable(oreon-system-manager src/main.cpp)
target_link_libraries(oreon-system-manager PRIVATE oreon_core)

option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS oreon-system-manager
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
