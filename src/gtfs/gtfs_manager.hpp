#pragma once
#include <string>
#include <vector>
#include "io/zip_file.hpp"

#include "agency.hpp"
#include "route.hpp"
#include "stop.hpp"

namespace gtfs {
    class GtfsManager {
    public:
        void load_feed(const std::string &zip_path);

        [[nodiscard]] std::vector<Stop> get_stops() const;

        [[nodiscard]] std::vector<Agency> get_agencies() const;

        [[nodiscard]] std::vector<Route> get_routes() const;

    private:
        std::vector<io::ZipFile> feeds;
    };
}
