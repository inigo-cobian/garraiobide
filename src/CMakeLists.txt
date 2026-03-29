add_library(gtfs_core
    core/logger.cpp
    io/http_client.cpp
        io/zip_file.cpp
    io/csv_reader.cpp
    io/geojson_reader.cpp
    io/temp_file.cpp
    gtfs/gtfs_manager.cpp
)

target_include_directories(gtfs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog REQUIRED)
find_package(cpr REQUIRED)
find_package(libzip REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fast-cpp-csv-parser REQUIRED)

target_link_libraries(gtfs_core
    PUBLIC
    spdlog::spdlog
    cpr::cpr
    libzip::zip
    nlohmann_json::nlohmann_json
)

add_executable(gtfs_app main.cpp)
target_link_libraries(gtfs_app PRIVATE gtfs_core)