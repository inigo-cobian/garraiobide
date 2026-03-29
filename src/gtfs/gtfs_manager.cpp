#include "gtfs_manager.hpp"
#include "../io/zip_reader.hpp"

namespace gtfs {

void GtfsManager::load_feed(const std::string& zip_path) {
    io::ZipFile zip_reader;
    zip_reader.open_file(zip_path);
}

}