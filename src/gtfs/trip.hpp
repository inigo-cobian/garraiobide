#pragma once

#include <string>

namespace gtfs {
    class Trip {
    private:
        std::string id;
        std::string routeId;
        std::string headsign;
        int directionId;
        std::string shapeId;

    public:
        explicit Trip(std::string id, std::string routeId, std::string headsign, int directionId, std::string shapeId);

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_route_id() const;

        [[nodiscard]] const std::string &get_headsign() const;

        [[nodiscard]] const int &get_direction_id() const;

        [[nodiscard]] const std::string &get_shape_id() const;

        std::strong_ordering operator<=>(const Trip& other) const {
            return id <=> other.id;
        }
    };
}
