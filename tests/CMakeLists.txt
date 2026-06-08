find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# Make mock shell scripts executable at configure time so ctest can run them.
file(GLOB _mocks "${CMAKE_CURRENT_SOURCE_DIR}/mocks/*")
foreach(_s ${_mocks})
    file(CHMOD "${_s}" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
endforeach()

# Helper: create a Qt test executable, register it with CTest, and force the
# offscreen platform so the suite runs on headless CI machines.
function(add_qt_test _name)
    qt_add_executable(${_name} ${ARGN})
    target_link_libraries(${_name} PRIVATE Qt6::Test Qt6::Widgets oreon_core)
    add_test(NAME ${_name} COMMAND ${_name})
    set_tests_properties(${_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

add_qt_test(test_sidebar     test_sidebar.cpp)
add_qt_test(test_pages       test_pages.cpp)

# Integration test needs the mock binaries on PATH ahead of the real tools.
qt_add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE Qt6::Test Qt6::Widgets oreon_core)
target_compile_definitions(test_integration PRIVATE
    MOCK_DIR="${CMAKE_CURRENT_SOURCE_DIR}/mocks"
)
add_test(NAME test_integration COMMAND test_integration)
set_tests_properties(test_integration PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
