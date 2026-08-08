// Usage: gtfs_ingest [--postgis <conn_string>] [--data <dir>] <path-to-gtfs.zip>
//
// Reads a GTFS ZIP archive and ingests transit data.
//
// Output modes (at least one required):
//   --data <dir>    Write layers as GeoJSON files (backward-compatible mode)
//   --postgis <cs>  Write typed entities to PostGIS via TransitRepository
//
// On success: prints summary to stdout, exits 0.
// On failure: prints error to stderr, exits 1.

#include <cstdlib>
#include <iostream>
#include <string>

#include <args.hxx>
#include <zip.h>

#include "adapters/ingestion/gtfs/csv_parser.h"
#include "adapters/ingestion/gtfs/gtfs_entity_parser.h"
#include "adapters/ingestion/gtfs/gtfs_parser.h"
#include "adapters/persistence/file_persistence_adapter.h"
#include "adapters/persistence/postgis_transit_repository.h"
#include "core/domain/entity_conversion.h"

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
    args::ArgumentParser parser("gtfs_ingest - ingest GTFS ZIP into GeoJSON or PostGIS");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::Positional<std::string> zip_path(parser, "ZIP_PATH",
                                           "Path to GTFS ZIP archive");
    args::ValueFlag<std::string> data_flag(parser, "DIR",
        "Write layers as GeoJSON to this directory", {'d', "data"});
    args::ValueFlag<std::string> postgis_flag(parser, "CONN",
        "Write typed entities to PostGIS (connection string)", {"postgis"});

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

    // Default to file mode if neither flag is given (backward compat)
    bool use_file = data_flag || (!data_flag && !postgis_flag);
    bool use_postgis = static_cast<bool>(postgis_flag);
    std::string data_dir = data_flag ? args::get(data_flag) : "data/";

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

    // ── File mode: produce layers (backward-compatible) ───────────────
    if (use_file) {
        auto result = parse_gtfs_feed(feed);
        if (!result) {
            std::cerr << "Error: failed to parse GTFS feed.\n";
            return EXIT_FAILURE;
        }

        garraiobide::adapters::persistence::FilePersistenceAdapter persistence(data_dir);

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

        std::cout << "File mode: ingested " << result->routes.features.size()
                  << " routes and " << result->stops.features.size() << " stops.\n";
    }

    // ── PostGIS mode: produce typed entities ──────────────────────────
    if (use_postgis) {
        auto entities = parse_gtfs_entities(feed);
        if (!entities) {
            std::cerr << "Error: failed to parse GTFS entities.\n";
            return EXIT_FAILURE;
        }

        try {
            garraiobide::adapters::persistence::PostgisTransitRepository repo(
                args::get(postgis_flag));

            // Save agencies
            for (const auto& agency : entities->agencies) {
                auto r = repo.save_agency(agency);
                if (!r && r.error() != garraiobide::core::ports::TransitRepositoryError::DuplicateEntity) {
                    std::cerr << "Error: failed to save agency " << agency.id << ".\n";
                    return EXIT_FAILURE;
                }
            }

            // Save stops (parents first, then children)
            for (const auto& stop : entities->stops) {
                if (stop.stop_type == garraiobide::core::domain::StopType::ParentStation ||
                    stop.stop_type == garraiobide::core::domain::StopType::Standalone) {
                    auto r = repo.save_stop(stop);
                    if (!r && r.error() != garraiobide::core::ports::TransitRepositoryError::DuplicateEntity) {
                        std::cerr << "Error: failed to save stop " << stop.id << ".\n";
                        return EXIT_FAILURE;
                    }
                }
            }
            for (const auto& stop : entities->stops) {
                if (stop.stop_type == garraiobide::core::domain::StopType::ChildStop) {
                    auto r = repo.save_stop(stop);
                    if (!r && r.error() != garraiobide::core::ports::TransitRepositoryError::DuplicateEntity) {
                        std::cerr << "Error: failed to save stop " << stop.id << ".\n";
                        return EXIT_FAILURE;
                    }
                }
            }

            // Save routes
            for (const auto& route : entities->routes) {
                auto r = repo.save_route(route);
                if (!r && r.error() != garraiobide::core::ports::TransitRepositoryError::DuplicateEntity) {
                    std::cerr << "Error: failed to save route " << route.id << ".\n";
                    return EXIT_FAILURE;
                }
            }

            // Save route-stop relationships
            for (const auto& [route_id, stop_ids] : entities->route_stop_sequences) {
                for (int i = 0; i < static_cast<int>(stop_ids.size()); ++i) {
                    [[maybe_unused]] auto r = repo.add_route_stop(route_id, stop_ids[i], i + 1);
                }
            }

            std::cout << "PostGIS mode: ingested " << entities->agencies.size()
                      << " agencies, " << entities->routes.size() << " routes, "
                      << entities->stops.size() << " stops.\n";

        } catch (const std::exception& e) {
            std::cerr << "Error: PostGIS connection failed: " << e.what() << "\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
