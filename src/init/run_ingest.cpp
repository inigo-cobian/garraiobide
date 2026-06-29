#include "run_ingest.hpp"

#include <iostream>
#include <mongocxx/exception/exception.hpp>

#include "data/mongo/mongodb_manager.hpp"
#include "data/postgis/postgis_manager.hpp"

namespace init {
    void run_ingest(core::IngestConfig &config) {
        try {
            data::MongoDBManager::Config mongoCfg;
            mongoCfg.uri = config.getMongoUri();
            data::MongoDBManager mongoManager(mongoCfg);

            std::string pgUri = config.getPostgresConnectionString();
            data::PostgisManager pgManager(pgUri);

            std::cout << "Database connections validated successfully.\n";
        } catch (const mongocxx::exception &e) {
            std::cerr << "MongoDB connection error: " << e.what() << "\n";
            std::exit(EXIT_FAILURE);
        } catch (const pqxx::broken_connection &e) {
            std::cerr << "PostGIS connection error: " << e.what() << "\n";
            std::exit(EXIT_FAILURE);
        } catch (const std::exception &e) {
            std::cerr << "Unexpected error: " << e.what() << "\n";
            std::exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
}
