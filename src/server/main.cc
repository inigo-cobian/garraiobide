#include <iostream>
#include <memory>

#include <args.hxx>

#include "../adapters/http/http_adapter.h"
#include "../adapters/ingestion/gtfs/gtfs_ingestion_adapter.h"
#include "../adapters/persistence/file_persistence_adapter.h"
#include "../adapters/persistence/mongo_persistence_adapter.h"
#include "../adapters/persistence/postgis_persistence_adapter.h"
#include "../adapters/persistence/postgis_transit_repository.h"
#include "../adapters/ui/mock_presentation_adapter.h"
#include "../app/layer_service.h"

int main(int argc, char* argv[]) {
    args::ArgumentParser parser("API Server",
                                "Garraiobide map API server");
    args::HelpFlag help(parser, "help", "Display this help menu",
                        {'h', "help"});
    args::ValueFlag<std::uint16_t> port_flag(
        parser, "PORT", "Port to listen on", {'p', "port"}, 8080);
    args::ValueFlag<std::string> data_flag(
        parser, "DataDir", "Directory of the data", {'d', "data"}, "data/");
    args::ValueFlag<std::string> postgis_flag(
        parser, "CONN", "PostGIS connection string (uses PostGIS as primary backend)",
        {"postgis"});
    args::ValueFlag<std::string> spatial_backend_flag(
        parser, "BACKEND",
        "Spatial query backend: postgis (default) or mongodb",
        {"spatial-backend"}, "postgis");
    args::ValueFlag<std::string> mongo_flag(
        parser, "URI", "MongoDB connection URI (for mongodb spatial backend)",
        {"mongo"});
    args::ValueFlag<std::string> mongo_db_flag(
        parser, "DB", "MongoDB database name", {"mongo-db"}, "garraiobide");

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::ParseError& e) {
        std::cerr << e.what() << "\n";
        std::cerr << parser;
        return 1;
    }

    const std::uint16_t port = args::get(port_flag);
    const std::string spatial_backend = args::get(spatial_backend_flag);

    // Wire adapters based on backend selection.
    garraiobide::adapters::ingestion::gtfs::GtfsIngestionAdapter ingestion;
    garraiobide::adapters::ui::MockPresentationAdapter presentation;

    std::unique_ptr<garraiobide::adapters::persistence::PostgisTransitRepository> transit_repo;
    std::unique_ptr<garraiobide::core::ports::PersistencePort> persistence;

    if (postgis_flag && spatial_backend == "mongodb") {
        // PostGIS for entity management, MongoDB for spatial map serving
        if (!mongo_flag) {
            std::cerr << "Error: --mongo is required when --spatial-backend=mongodb\n";
            return 1;
        }

        const std::string& mongo_uri = args::get(mongo_flag);
        const std::string& mongo_db = args::get(mongo_db_flag);

        try {
            persistence = std::make_unique<
                garraiobide::adapters::persistence::MongoPersistenceAdapter>(
                    mongo_uri, mongo_db);
            std::cout << "Using MongoDB spatial backend (" << mongo_db << ")\n";
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to MongoDB: " << e.what() << "\n";
            return 1;
        }

        // Also connect PostGIS for reference (used by tools, not for serving)
        try {
            transit_repo = std::make_unique<
                garraiobide::adapters::persistence::PostgisTransitRepository>(
                    args::get(postgis_flag));
            std::cout << "PostGIS connected for entity management\n";
        } catch (const std::exception& e) {
            std::cerr << "Warning: PostGIS not available: " << e.what() << "\n";
        }

    } else if (postgis_flag) {
        // PostGIS for everything (default when --postgis is provided)
        const std::string& conn_str = args::get(postgis_flag);
        try {
            transit_repo = std::make_unique<
                garraiobide::adapters::persistence::PostgisTransitRepository>(conn_str);
            persistence = std::make_unique<
                garraiobide::adapters::persistence::PostgisPersistenceAdapter>(*transit_repo);
            std::cout << "Using PostGIS backend\n";
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to PostGIS: " << e.what() << "\n";
            return 1;
        }

    } else if (spatial_backend == "mongodb" && mongo_flag) {
        // MongoDB-only mode (no PostGIS)
        const std::string& mongo_uri = args::get(mongo_flag);
        const std::string& mongo_db = args::get(mongo_db_flag);
        try {
            persistence = std::make_unique<
                garraiobide::adapters::persistence::MongoPersistenceAdapter>(
                    mongo_uri, mongo_db);
            std::cout << "Using MongoDB backend (" << mongo_db << ")\n";
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to MongoDB: " << e.what() << "\n";
            return 1;
        }

    } else {
        // File backend (default)
        const std::string data_dir = args::get(data_flag);
        persistence = std::make_unique<
            garraiobide::adapters::persistence::FilePersistenceAdapter>(data_dir);
        std::cout << "Using file backend (" << data_dir << ")\n";
    }

    // Create application service.
    garraiobide::app::LayerService layer_service(
        ingestion, *persistence, presentation);

    // Create HTTP adapter and start listening.
    garraiobide::adapters::http::HttpAdapter http_adapter(layer_service);

    std::cout << "Listening on http://localhost:" << port << "\n";
    http_adapter.listen(port);

    return 0;
}
