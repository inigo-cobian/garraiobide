#pragma once
#include <string>

namespace gtfs {

class GtfsManager {
public:
    void load_feed(const std::string& zip_path);
};

}