#pragma once
#include <string>
#include <vector>

namespace gtfs {

class GtfsManager {

public:
    void load_feed(const std::string &zip_path);
private:
    const std::vector<std::string> RELEVANT_GTFS_FILES = {"stops.txt", "shapes.txt"};
};

}
