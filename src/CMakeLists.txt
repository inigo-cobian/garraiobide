add_library(garraiobide_core
    core/args.cpp
    core/config.cpp
    core/logger.cpp
    core/resource.cpp
    core/stop.cpp
    core/line.cpp
    core/stop_in_line.cpp
    data/mongo/mongodb_manager.cpp
    data/mongo/stop_repository.cpp
    data/postgis/postgis_manager.cpp
    io/http_client.cpp
    io/zip_file.cpp
    io/csv_reader.cpp
    io/geojson_reader.cpp
    io/temp_file.cpp
    gtfs/gtfs_manager.cpp
    gtfs/stop.cpp
    gtfs/agency.cpp
    gtfs/route.cpp
    gtfs/shape.cpp
    gtfs/trip.cpp
    gtfs/stop_time.cpp
    init/run.cpp
    init/run_stats.cpp
    init/run_ingest.cpp
    ui/color.cpp
)

target_include_directories(garraiobide_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog REQUIRED)
find_package(cpr REQUIRED)
find_package(libzip REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fast-cpp-csv-parser REQUIRED)
find_package(GDAL REQUIRED)
find_package(args REQUIRED)
find_package(mongocxx REQUIRED)
find_package(libpqxx REQUIRED)

target_link_libraries(garraiobide_core
    PUBLIC
    spdlog::spdlog
    cpr::cpr
    libzip::zip
    nlohmann_json::nlohmann_json
    GDAL::GDAL
    taywee::args
    mongo::mongocxx_static
    libpqxx::pqxx
)

add_executable(garraiobide_app main.cpp)
target_link_libraries(garraiobide_app PRIVATE garraiobide_core)