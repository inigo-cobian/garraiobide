#include "run_ingest.hpp"

#include <iostream>
#include <mongocxx/exception/exception.hpp>

#include "core/resource.hpp"
#include "core/stop.hpp"
#include "data/mongo/mongodb_manager.hpp"
#include "data/postgis/postgis_manager.hpp"
#include "gtfs/gtfs_manager.hpp"
#include "io/http_client.hpp"
#include "io/temp_file.hpp"

namespace init {
    void run_ingest(core::IngestConfig &config) {
        try {
            data::MongoDBManager::Config mongoCfg;
            mongoCfg.uri = config.getMongoUri();
            data::MongoDBManager mongoManager(mongoCfg);

            std::string pgUri = config.getPostgresConnectionString();
            data::PostgisManager pgManager(pgUri);

            if (config.getType() == "gtfs") {
                gtfs::GtfsManager gtfs_manager{};
                gtfs_manager.load_feed(config.getName(), config.getUrl());

                auto gtfs_stops = gtfs_manager.get_stops();
                std::vector<core::Stop> stops{};
                for (auto &gtfs_stop: gtfs_stops) {
                    core::Stop stop(gtfs_stop, config.getName());
                    stops.push_back(stop);
                }

                for (auto stop: stops) {
                    std::cout << stop.get_id() << "    " << stop.get_name() << std::endl;
                    pgManager.insertStop(stop);
                }
            }

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

        core::Resource res(config.getName(), config.getUrl(), core::ResourceType::gtfs);
        auto j = res.as_json();
        std::cout << std::setw(4) << j << std::endl;

        exit(EXIT_SUCCESS);
    }
}
