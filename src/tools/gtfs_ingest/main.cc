// Usage: gtfs_ingest <path-to-gtfs.zip>
//
// Reads a GTFS ZIP archive, produces route and stop layers,
// and writes them to the data/ directory as GeoJSON files.
//
// On success: prints "Ingested N routes and M stops." to stdout, exits 0.
// On failure: prints error to stderr, exits 1.

#include <cstdlib>
#include <iostream>
#include <string>

#include <args.hxx>
#include <zip.h>

#include "src/adapters/ingestion/gtfs/csv_parser.h"
#include "src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "src/adapters/persistence/file_persistence_adapter.h"

namespace {

/// Read a single file from a ZIP archive. Returns empty string on failure.
std::string read_zip_file(const std::string& zip_path, const std::string& entry_name) {
    int errcode = 0;
    zip_t* archive = zip_open(zip_path.c_str(), ZIP_RDONLY, &errcode);
    if (!archive) return {};

    zip_int64_t index = zip_name_locate(archive, entry_name.c_str(), 0);
    if (index < 0) { zip_close(archive); return {}; }

    zip_stat_t stat;
    if (zip_stat_index(archive, static_cast<zip_uint64_t>(index), 0, &stat) != 0) {
        zip_close(archive);
        return {};
    }

    zip_file_t* file = zip_fopen_index(archive, static_cast<zip_uint64_t>(index), 0);
    if (!file) { zip_close(archive); return {}; }

    std::string contents(stat.size, '\0');
    zip_int64_t bytes_read = zip_fread(file, contents.data(), stat.size);
    zip_fclose(file);
    zip_close(archive);

    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != stat.size) return {};
    return contents;
}

bool zip_has_entry(const std::string& zip_path, const std::string& entry_name) {
    int errcode = 0;
    zip_t* archive = zip_open(zip_path.c_str(), ZIP_RDONLY, &errcode);
    if (!archive) return false;
    zip_int64_t index = zip_name_locate(archive, entry_name.c_str(), 0);
    zip_close(archive);
    return index >= 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    args::ArgumentParser parser("gtfs_ingest - ingest GTFS ZIP into GeoJSON");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::Positional<std::string> zip_path(parser, "ZIP_PATH",
                                           "Path to GTFS ZIP archive");

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << parser;
        return EXIT_SUCCESS;
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << "\n";
        std::cerr << parser;
        return EXIT_FAILURE;
    }

    if (!zip_path) {
        std::cerr << "Error: ZIP_PATH argument is required.\n";
        std::cerr << parser;
        return EXIT_FAILURE;
    }

    const std::string& path = args::get(zip_path);

    // Read required GTFS CSV files from the ZIP
    auto agency_csv = read_zip_file(path, "agency.txt");
    if (agency_csv.empty()) {
        std::cerr << "Error: could not read agency.txt from ZIP.\n";
        return EXIT_FAILURE;
    }

    auto routes_csv = read_zip_file(path, "routes.txt");
    if (routes_csv.empty()) {
        std::cerr << "Error: could not read routes.txt from ZIP.\n";
        return EXIT_FAILURE;
    }

    auto trips_csv = read_zip_file(path, "trips.txt");
    if (trips_csv.empty()) {
        std::cerr << "Error: could not read trips.txt from ZIP.\n";
        return EXIT_FAILURE;
    }

    auto stops_csv = read_zip_file(path, "stops.txt");
    if (stops_csv.empty()) {
        std::cerr << "Error: could not read stops.txt from ZIP.\n";
        return EXIT_FAILURE;
    }

    auto stop_times_csv = read_zip_file(path, "stop_times.txt");
    if (stop_times_csv.empty()) {
        std::cerr << "Error: could not read stop_times.txt from ZIP.\n";
        return EXIT_FAILURE;
    }

    // Optional shapes.txt
    std::string shapes_csv;
    if (zip_has_entry(path, "shapes.txt")) {
        shapes_csv = read_zip_file(path, "shapes.txt");
    }

    // Parse CSV files
    using namespace garraiobide::adapters::ingestion::gtfs;
    GtfsFeed feed;
    feed.agency = parse_csv(agency_csv);
    feed.routes = parse_csv(routes_csv);
    feed.trips = parse_csv(trips_csv);
    feed.stops = parse_csv(stops_csv);
    feed.stop_times = parse_csv(stop_times_csv);
    feed.shapes = parse_csv(shapes_csv);

    // Parse GTFS feed into domain layers
    auto result = parse_gtfs_feed(feed);
    if (!result) {
        std::cerr << "Error: failed to parse GTFS feed.\n";
        return EXIT_FAILURE;
    }

    // Save layers via FilePersistenceAdapter
    garraiobide::adapters::persistence::FilePersistenceAdapter persistence("data/");

    auto save_routes = persistence.save_layer(result->routes);
    if (!save_routes) {
        std::cerr << "Error: failed to save routes layer.\n";
        return EXIT_FAILURE;
    }

    auto save_stops = persistence.save_layer(result->stops);
    if (!save_stops) {
        std::cerr << "Error: failed to save stops layer.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Ingested " << result->routes.features.size() << " routes and "
              << result->stops.features.size() << " stops.\n";

    return EXIT_SUCCESS;
}
