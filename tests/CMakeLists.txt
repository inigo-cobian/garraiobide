find_package(GTest REQUIRED)

add_executable(gtfs_tests
    gtfs_test.cpp
)

target_link_libraries(gtfs_tests
    PRIVATE
    gtfs_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(gtfs_tests)