#include "trip.hpp"

#include <utility>

namespace gtfs {
    Trip::Trip(std::string id, std::string routeId, std::string headsign, int directionId, std::string shapeId)
        : id(std::move(id)), routeId(std::move(routeId)), headsign(std::move(headsign)), directionId(directionId),
          shapeId(std::move(shapeId)) {
    }

    const std::string &Trip::get_id() const {
        return id;
    }

    const std::string &Trip::get_route_id() const {
        return routeId;
    }

    const std::string &Trip::get_headsign() const {
        return headsign;
    }

    const int &Trip::get_direction_id() const {
        return directionId;
    }

    const std::string &Trip::get_shape_id() const {
        return shapeId;
    }
}
