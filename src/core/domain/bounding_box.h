#pragma once

#include "src/core/domain/coordinate.h"

namespace garraiobide::core::domain {

/// Axis-aligned bounding box defined by its south-west and north-east corners.
struct BoundingBox {
    Coordinate south_west;
    Coordinate north_east;

    [[nodiscard]] bool contains(const Coordinate& point) const {
        return point.latitude >= south_west.latitude &&
               point.latitude <= north_east.latitude &&
               point.longitude >= south_west.longitude &&
               point.longitude <= north_east.longitude;
    }

    [[nodiscard]] bool intersects(const BoundingBox& other) const {
        return !(other.north_east.latitude < south_west.latitude ||
                 other.south_west.latitude > north_east.latitude ||
                 other.north_east.longitude < south_west.longitude ||
                 other.south_west.longitude > north_east.longitude);
    }
};

}  // namespace garraiobide::core::domain
