#include "run_ingest.hpp"

#include <iostream>
#include <mongocxx/exception/exception.hpp>

#include "core/resource.hpp"
#include "core/stop.hpp"
#include "core/stop_in_line.hpp"
#include "core/trip.hpp"
#include "data/mongo/mongodb_manager.hpp"
#include "data/postgis/postgis_manager.hpp"
#include "gtfs/gtfs_manager.hpp"
#include "io/http_client.hpp"
#include "io/temp_file.hpp"

namespace init {
    OGRLineString findShapeByCode(const std::vector<gtfs::Shape> &shapes, std::string code) {
        for (auto shape: shapes) {
            if (shape.get_code() == code) {
                return shape.get_line();
            }
        }
        return OGRLineString{};
    }

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

                auto gtfs_trips = gtfs_manager.get_trips();
                auto gtfs_routes = gtfs_manager.get_routes();
                auto gtfs_shapes = gtfs_manager.get_shapes();
                auto gtfs_stop_times = gtfs_manager.get_stop_times();

                std::vector<core::Trip> trips{};
                for (auto gtfs_trip: gtfs_trips) {
                    OGRLineString shape = gtfs_shapes.has_value()
                                              ? findShapeByCode(gtfs_shapes.value(), gtfs_trip.get_shape_id())
                                              : OGRLineString{};
                    core::Trip trip(gtfs_trip.get_id(), shape, config.getName());
                    pgManager.insertTrip(trip);

                    gtfs_trip.build_ordered_stops(gtfs_stops, gtfs_stop_times);
                    std::cout << trip.get_id() << ":" << gtfs_trip.get_headsign() << std::endl;
                    for (const auto &ordered_stop: gtfs_trip.get_ordered_stops()) {
                        core::StopInLine stopInLine(ordered_stop.first, ordered_stop.second.get_id(),
                                                    gtfs_trip.get_id(), config.getName());
                        pgManager.insertStopInLine(stopInLine);
                    }
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
