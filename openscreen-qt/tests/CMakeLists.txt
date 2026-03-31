find_package(GTest REQUIRED)

include(GoogleTest)

# Macro: add a test executable from a single source file.
# Usage: openscreen_add_test(test_settings)  ->  builds tests/test_settings.cpp
macro(openscreen_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

    target_link_libraries(${TEST_NAME} PRIVATE
        GTest::gtest_main
        openscreen_core
    )

    gtest_discover_tests(${TEST_NAME})
endmacro()

# Discover all test_*.cpp files and register them automatically.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

foreach(test_file IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_file} NAME_WE)
    openscreen_add_test(${test_name})
endforeach()
