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
        std::cout << stop.getName() << std::endl;
    }

    return 0;
}
