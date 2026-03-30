#include "gtfs_manager.hpp"
#include "../io/zip_file.hpp"

namespace gtfs {

void GtfsManager::load_feed(const std::string& zip_path) {
    feeds.push_back(io::ZipFile(zip_path));

}

std::string GtfsManager::get_stops() {
    // TODO manage n feeds
    return feeds.at(0).get_file_content("stops.txt");
}
}
