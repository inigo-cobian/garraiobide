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

struct MultiLineString {
    std::vector<std::vector<Coordinate>> lines;
};

struct Polygon {
    /// Outer ring followed by optional inner rings (holes).
    std::vector<std::vector<Coordinate>> rings;
};

/// A geometry is one of the supported spatial primitives.
using Geometry = std::variant<Point, LineString, MultiLineString, Polygon>;

}  // namespace garraiobide::core::domain
