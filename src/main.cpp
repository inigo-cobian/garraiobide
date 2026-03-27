#include <iostream>
#include <fstream>
#include <io/http_client.hpp>
#include <io/temp_file.hpp>

#include "core/logger.hpp"
#include "gtfs/gtfs_manager.hpp"

int main() {
    core::Logger::init();

    std::string metroBilbaoGtfsUrl = "https://cms.metrobilbao.eus/get/open_data/horarios/eu";
    auto metroBilbaoGtfs = io::HttpClient::download(metroBilbaoGtfsUrl);
    auto tmpfile = io::TempFile("tmp", ".zip");

    std::ofstream out(tmpfile.getPath());

    out << metroBilbaoGtfs;

    gtfs::GtfsManager manager;
    manager.load_feed(tmpfile.getPath());

    return 0;
}