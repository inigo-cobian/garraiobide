find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(garraiobide_tests
    domain_test.cc
    e2e_coordinate_consistency_test.cc
    file_persistence_property_test.cc
    file_persistence_test.cc
    geojson_serializer_property_test.cc
    geojson_serializer_test.cc
    gtfs_agency_name_test.cc
    gtfs_bbox_filter_test.cc
    gtfs_csv_parser_test.cc
    gtfs_ingest_integration_test.cc
    gtfs_metadata_test.cc
    gtfs_persistence_roundtrip_test.cc
    gtfs_route_geometry_test.cc
    gtfs_stop_classification_test.cc
    http_adapter_test.cc
    layer_service_test.cc
    parse_args_test.cc
)

target_include_directories(garraiobide_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(garraiobide_tests
    PRIVATE
    garraiobide_src
    GTest::gtest_main
    GTest::gtest
    GTest::gmock_main
    GTest::gmock
)

enable_testing()
gtest_discover_tests(garraiobide_tests)