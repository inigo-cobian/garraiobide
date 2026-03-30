#include "gtfs_manager.hpp"
#include "../io/zip_file.hpp"

namespace gtfs {

void GtfsManager::load_feed(const std::string& zip_path) {
    io::ZipFile zip_reader(zip_path);
}

}