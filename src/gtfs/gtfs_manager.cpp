#include "gtfs_manager.hpp"
#include "../io/zip_reader.hpp"
#include "../io/csv_reader.hpp"

namespace gtfs {

void GtfsManager::load_feed(const std::string& zip_path) {
    io::ZipReader::extract(zip_path, "data");
}

}