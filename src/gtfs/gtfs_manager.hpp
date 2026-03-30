#pragma once
#include <string>
#include <vector>
#include "io/zip_file.hpp"


#include "stop.hpp"

namespace gtfs {

class GtfsManager {

public:
    void load_feed(const std::string &zip_path);
    [[nodiscard]] std::vector<Stop> get_stops() const;
    [[nodiscard]] std::string get_agency();
private:
    std::vector<io::ZipFile> feeds;
};

}
