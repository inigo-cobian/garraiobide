find_package(GTest REQUIRED)
include(GoogleTest)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/files" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(garraiobide_tests
    csv_reader_test.cpp
    main_test.cpp
    zip_file_test.cpp
    geojson_reader_test.cpp
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