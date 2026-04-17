#include <iostream>
#include <fstream>

#include "core/logger.hpp"
#include "gtfs/gtfs_manager.hpp"
#include "io/http_client.hpp"
#include "io/temp_file.hpp"

int main() {
    core::Logger::init();

    std::string metroBilbaoGtfsUrl = "https://cms.metrobilbao.eus/get/open_data/horarios/eu";
    auto metroBilbaoGtfs = io::HttpClient::download(metroBilbaoGtfsUrl);
    auto tmpfile = io::TempFile("tmp", ".zip");

    std::ofstream out(tmpfile.getPath());

    out << metroBilbaoGtfs;

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
    for (const auto& shape : shapes) {
        auto s = shape.get_line().exportToWkt();
        std::cout << s << std::endl;
    }

    return 0;
}
