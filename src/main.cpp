#include <iostream>
#include <fstream>

#include "core/args.hpp"
#include "core/config.hpp"
#include "core/logger.hpp"
#include "core/resource.hpp"
#include "data/mongodb_manager.hpp"
#include "gtfs/gtfs_manager.hpp"
#include "io/http_client.hpp"
#include "io/temp_file.hpp"

int main(int argc, char *argv[]) {

    core::Args::init(argc, argv);
    core::Logger::init();
    core::Config::init();

    std::string metroBilbaoGtfsUrl = "https://cms.metrobilbao.eus/get/open_data/horarios/eu";
    auto metroBilbaoGtfs = io::HttpClient::download(metroBilbaoGtfsUrl);
    std::string renfeMdBilbaoGtfsUrl = "https://opendata.euskadi.eus/transport/moveuskadi/renfe_media/gtfs_renfe_media.zip";
    auto renfeMdBilbaoGtfs = io::HttpClient::download(renfeMdBilbaoGtfsUrl);
    std::string euskotrenGtfsUrl = "https://opendata.euskadi.eus/transport/moveuskadi/euskotren/gtfs_euskotren.zip";
    auto euskotrenGtfs = io::HttpClient::download(euskotrenGtfsUrl);

    core::Resource res("metro-bilbao", metroBilbaoGtfsUrl, core::ResourceType::gtfs);
    auto j = res.as_json();
    std::cout << std::setw(4) << j << std::endl;

    auto dir = core::Config::get_installation_dir() + "/resources/" + res.get_name() + ".json";
    std::ofstream o(dir);
    //o.open(core::Config::get_resource_dir() + res.get_name() + ".json");

    if (!o.is_open()) {
        throw std::runtime_error("Could not open resource file: " + dir);
    }
    o << std::setw(4) << j << std::endl;

    auto mongo_cfg = data::MongoDBManager::Config{};
    mongo_cfg.uri = "mongodb://localhost:27017";
    data::MongoDBManager mongo_mgr(std::move(mongo_cfg));
    
    return 0;

    auto tmpfile = io::TempFile(".zip");

    std::ofstream out(tmpfile.getPath());

    out << euskotrenGtfs;

    gtfs::GtfsManager manager;
    manager.load_feed(tmpfile.getPath());

    auto stops = manager.get_stops();
    for (const auto& stop : stops) {
        std::cout << stop.get_name() << std::endl;
    }

    auto agencies = manager.get_agencies();
    for (const auto& agency : agencies) {
        std::cout << agency.get_name() << std::endl;
    }

    auto routes = manager.get_routes();
    for (const auto& route : routes) {
        std::cout << route.get_name() << std::endl;
    }

    auto shapes = manager.get_shapes();
    if (!shapes.has_value()) {
        return 1;
    }
    for (const auto& shape : shapes.value()) {
        auto s = shape.get_line().exportToWkt();
        std::cout << s << std::endl;
    }

    auto trips = manager.get_trips();
    for (const auto& trip : trips) {
        auto s = trip.get_shape_id();
        std::cout << s << std::endl;
    }

    auto stop_times = manager.get_stop_times();
    for (const auto& stop_time : stop_times) {
        std::cout << stop_time.get_trip_id() << ":" << stop_time.get_stop_sequence() << std::endl;
    }

    auto result = gtfs::GtfsManager::get_trip_ordered_stops(stops, trips, stop_times);
    for (const auto& res : result) {
        std::cout << res.first.get_route_id() << std::endl;
        for (const auto& stop : res.second) {
            std::cout << "     " << stop.get_name() << std::endl;
        }
    }

    return 0;
}
