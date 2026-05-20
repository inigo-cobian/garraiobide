add_library(garraiobide_core
    core/logger.cpp
    core/resource.cpp
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
)

target_include_directories(garraiobide_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog REQUIRED)
find_package(cpr REQUIRED)
find_package(libzip REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fast-cpp-csv-parser REQUIRED)
find_package(GDAL REQUIRED)

target_link_libraries(garraiobide_core
    PUBLIC
    spdlog::spdlog
    cpr::cpr
    libzip::zip
    nlohmann_json::nlohmann_json
    GDAL::GDAL
)

add_executable(garraiobide_app main.cpp)
target_link_libraries(garraiobide_app PRIVATE garraiobide_core)