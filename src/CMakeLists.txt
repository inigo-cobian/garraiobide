add_library(garraiobide_src
    adapters/cli/mock_cli_adapter.cc
    adapters/http/geojson_serializer.cc
    adapters/http/http_adapter.cc
    adapters/ingestion/mock_ingestion_adapter.cc
    adapters/ingestion/gtfs/gtfs_parser.cc
    adapters/ingestion/gtfs/gtfs_ingestion_adapter.cc
    adapters/persistence/file_persistence_adapter.cc
    adapters/persistence/mock_persistence_adapter.cc
    adapters/ui/mock_presentation_adapter.cc
    app/layer_service.cc
    app/parse_args.cc
    core/domain/layer.cc   
)

target_include_directories(garraiobide_src PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(args REQUIRED)
find_package(libzip REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)

target_link_libraries(garraiobide_src
    PUBLIC
    taywee::args
    libzip::zip
    nlohmann_json::nlohmann_json
    httplib::httplib
)

add_executable(garraiobide_app main.cc)
target_link_libraries(garraiobide_app PRIVATE garraiobide_src)