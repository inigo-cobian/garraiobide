// Usage: sync_mongo --postgis <conn> --mongo <uri> [--db <name>]
//
// Reads all transit entities from PostGIS, converts them to layers,
// and writes them to MongoDB (drop + reinsert). Records sync metadata
// in the PostGIS sync_metadata table.
//
// On success: prints summary, exits 0.
// On failure: prints error to stderr, exits 1.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include <args.hxx>
#include <pqxx/pqxx>

#include "adapters/persistence/mongo_persistence_adapter.h"
#include "adapters/persistence/postgis_transit_repository.h"
#include "core/domain/entity_conversion.h"

namespace {

/// Record sync start in PostGIS sync_metadata table. Returns the sync id.
int record_sync_start(pqxx::connection& conn) {
    pqxx::work txn(conn);
    auto result = txn.exec(
        "INSERT INTO sync_metadata (sync_type, status) "
        "VALUES ('full', 'running') RETURNING id");
    txn.commit();
    return result[0][0].as<int>();
}

/// Record sync completion.
void record_sync_complete(pqxx::connection& conn, int sync_id, int records) {
    pqxx::work txn(conn);
    txn.exec_params(
        "UPDATE sync_metadata SET status = 'completed', "
        "completed_at = NOW(), records_synced = $1 WHERE id = $2",
        records, sync_id);
    txn.commit();
}

/// Record sync failure.
void record_sync_failed(pqxx::connection& conn, int sync_id,
                        const std::string& error) {
    pqxx::work txn(conn);
    txn.exec_params(
        "UPDATE sync_metadata SET status = 'failed', "
        "completed_at = NOW(), error_message = $1 WHERE id = $2",
        error, sync_id);
    txn.commit();
}

}  // namespace

int main(int argc, char* argv[]) {
    args::ArgumentParser parser("sync_mongo - sync PostGIS to MongoDB");
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<std::string> postgis_flag(parser, "CONN",
        "PostGIS connection string", {"postgis"});
    args::ValueFlag<std::string> mongo_flag(parser, "URI",
        "MongoDB connection URI", {"mongo"});
    args::ValueFlag<std::string> db_flag(parser, "DB",
        "MongoDB database name", {"db"}, "garraiobide");

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

    if (!postgis_flag || !mongo_flag) {
        std::cerr << "Error: both --postgis and --mongo are required.\n";
        std::cerr << parser;
        return EXIT_FAILURE;
    }

    const std::string& postgis_conn = args::get(postgis_flag);
    const std::string& mongo_uri = args::get(mongo_flag);
    const std::string& mongo_db = args::get(db_flag);

    int sync_id = -1;
    std::unique_ptr<pqxx::connection> meta_conn;

    try {
        // Open a connection for sync metadata tracking
        meta_conn = std::make_unique<pqxx::connection>(postgis_conn);
        sync_id = record_sync_start(*meta_conn);
        std::cout << "Sync started (id=" << sync_id << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to PostGIS: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    try {
        // Read from PostGIS
        garraiobide::adapters::persistence::PostgisTransitRepository repo(postgis_conn);

        auto routes_result = repo.list_routes("");
        auto stops_result = repo.list_stops("");

        if (!routes_result || !stops_result) {
            throw std::runtime_error("Failed to read entities from PostGIS");
        }

        // Convert to layers
        std::string prefix = "transit";
        auto agencies = repo.list_agencies();
        if (agencies && !agencies->empty()) {
            prefix = agencies->at(0).id;
        }

        auto routes_layer = garraiobide::core::domain::routes_to_layer(
            *routes_result, prefix + "_routes");
        auto stops_layer = garraiobide::core::domain::stops_to_layer(
            *stops_result, prefix + "_stops");

        // Write to MongoDB (drop + reinsert via MongoPersistenceAdapter)
        garraiobide::adapters::persistence::MongoPersistenceAdapter mongo(
            mongo_uri, mongo_db);

        // Remove existing layers then re-save
        [[maybe_unused]] auto r1 = mongo.remove_layer(routes_layer.name);
        [[maybe_unused]] auto r2 = mongo.remove_layer(stops_layer.name);

        auto save_routes = mongo.save_layer(routes_layer);
        if (!save_routes) {
            throw std::runtime_error("Failed to save routes layer to MongoDB");
        }

        auto save_stops = mongo.save_layer(stops_layer);
        if (!save_stops) {
            throw std::runtime_error("Failed to save stops layer to MongoDB");
        }

        int total_records = static_cast<int>(
            routes_layer.features.size() + stops_layer.features.size());

        // Record success
        record_sync_complete(*meta_conn, sync_id, total_records);

        std::cout << "Sync complete: " << routes_layer.features.size()
                  << " routes, " << stops_layer.features.size()
                  << " stops synced to MongoDB.\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "Sync failed: " << e.what() << "\n";
        if (meta_conn && sync_id >= 0) {
            record_sync_failed(*meta_conn, sync_id, e.what());
        }
        return EXIT_FAILURE;
    }
}
