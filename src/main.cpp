#include "core/logger.hpp"
#include "gtfs/gtfs_manager.hpp"

int main() {
    core::Logger::init();

    gtfs::GtfsManager manager;
    manager.load_feed("data/feed.zip");

    return 0;
}