find_package(GTest REQUIRED)
include(GoogleTest)

file(COPY "core/files" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "io/files" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "gtfs/files" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(garraiobide_tests
        io/csv_reader_test.cpp
        io/main_test.cpp
        io/zip_file_test.cpp
        io/geojson_reader_test.cpp
        core/resource_test.cpp
)

target_include_directories(garraiobide_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(garraiobide_tests
    PRIVATE
    garraiobide_core
    GTest::gtest_main
    GTest::gtest
    GTest::gmock_main
    GTest::gmock
)

enable_testing()
gtest_discover_tests(garraiobide_tests)