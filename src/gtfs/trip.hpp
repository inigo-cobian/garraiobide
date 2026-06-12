#pragma once

#include <map>
#include <optional>
#include <string>

#include "stop.hpp"

namespace gtfs {
    class StopTime;

    class Trip {
    private:
        std::string id;
        std::string routeId;
        std::string headsign;
        std::optional<int> directionId;
        std::string shapeId;
        std::map<int, Stop> orderedStops;

    public:
        explicit Trip(std::string id, std::string routeId, std::string headsign, std::optional<int> directionId, std::string shapeId);

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_route_id() const;

        [[nodiscard]] const std::string &get_headsign() const;

        [[nodiscard]] const std::optional<int> &get_direction_id() const;

        [[nodiscard]] const std::string &get_shape_id() const;

        [[nodiscard]] std::map<int, Stop> get_ordered_stops() const;

        std::strong_ordering operator<=>(const Trip& other) const {
            return id <=> other.id;
        }

        void build_ordered_stops(const std::vector<Stop>&, const std::vector<StopTime>&);
    };
}
