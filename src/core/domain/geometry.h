#pragma once

#include <variant>
#include <vector>

#include "coordinate.h"

namespace garraiobide::core::domain {

struct Point {
    Coordinate position;
};

struct LineString {
    std::vector<Coordinate> vertices;
};

struct Polygon {
    /// Outer ring followed by optional inner rings (holes).
    std::vector<std::vector<Coordinate>> rings;
};

/// A geometry is one of the supported spatial primitives.
using Geometry = std::variant<Point, LineString, Polygon>;

}  // namespace garraiobide::core::domain
