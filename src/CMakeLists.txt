add_library(garraiobide_src
        adapters/cli/mock_cli_adapter.cc
        adapters/http/geojson_serializer.cc
        adapters/http/http_adapter.cc
        adapters/ingestion/gtfs/gtfs_parser.cc
        adapters/ingestion/gtfs/gtfs_ingestion_adapter.cc
        adapters/persistence/file_persistence_adapter.cc
        adapters/persistence/mock_persistence_adapter.cc
        adapters/persistence/mongo_persistence_adapter.cc
        adapters/ui/mock_presentation_adapter.cc
        app/layer_service.cc
        app/parse_args.cc
        core/domain/layer.cc
)

target_include_directories(garraiobide_src PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(libzip REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)
find_package(args REQUIRED)
find_package(mongocxx REQUIRED)

target_link_libraries(garraiobide_src
        PUBLIC
        taywee::args
        libzip::zip
        nlohmann_json::nlohmann_json
        httplib::httplib
        mongo::mongocxx_static
        mongo::bsoncxx_static
)

add_executable(garraiobide_app server/main.cc)
target_link_libraries(garraiobide_app PRIVATE garraiobide_src)
