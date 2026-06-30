#pragma once
#include <string>
#include <vector>
#include "io/zip_file.hpp"

#include "agency.hpp"
#include "route.hpp"
#include "stop.hpp"
#include "shape.hpp"
#include "stop_time.hpp"
#include "trip.hpp"

namespace gtfs {
    class GtfsManager {
        std::vector<std::pair<std::string, io::ZipFile>> feeds;

    public:
        void load_feed(const std::string &name, const std::string &gtfsUrl);

        [[nodiscard]] std::vector<Stop> get_stops() const;

        [[nodiscard]] std::vector<Agency> get_agencies() const;

        [[nodiscard]] std::vector<Route> get_routes() const;

        [[nodiscard]] std::optional<std::vector<Shape> > get_shapes() const;

        [[nodiscard]] std::vector<Trip> get_trips() const;

        [[nodiscard]] std::vector<StopTime> get_stop_times() const;
    };
}
