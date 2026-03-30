#pragma once
#include <memory>
#include <string>
#include <vector>
#include <io/zip_file.hpp>

namespace gtfs {

class GtfsManager {

public:
    void load_feed(const std::string &zip_path);
    [[nodiscard]] std::string get_stops();
private:
    std::vector<io::ZipFile> feeds;
    const std::vector<std::string> RELEVANT_GTFS_FILES = {"stops.txt", "shapes.txt"};
};

}
