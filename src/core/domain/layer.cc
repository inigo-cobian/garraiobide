#include "layer.h"

#include <algorithm>
#include <limits>

namespace garraiobide::core::domain {

namespace {

struct CoordinateCollector {
    std::vector<Coordinate>& coords;

    void operator()(const Point& p) { coords.push_back(p.position); }

    void operator()(const LineString& ls) {
        coords.insert(coords.end(), ls.vertices.begin(), ls.vertices.end());
    }

    void operator()(const Polygon& poly) {
        for (const auto& ring : poly.rings) {
            coords.insert(coords.end(), ring.begin(), ring.end());
        }
    }
};

}  // namespace

std::optional<BoundingBox> Layer::envelope() const {
    std::vector<Coordinate> all_coords;
    all_coords.reserve(features.size());

    for (const auto& feature : features) {
        std::visit(CoordinateCollector{all_coords}, feature.geometry);
    }

    if (all_coords.empty()) {
        return std::nullopt;
    }

    double min_lat = std::numeric_limits<double>::max();
    double max_lat = std::numeric_limits<double>::lowest();
    double min_lon = std::numeric_limits<double>::max();
    double max_lon = std::numeric_limits<double>::lowest();

    for (const auto& c : all_coords) {
        min_lat = std::min(min_lat, c.latitude);
        max_lat = std::max(max_lat, c.latitude);
        min_lon = std::min(min_lon, c.longitude);
        max_lon = std::max(max_lon, c.longitude);
    }

    return BoundingBox{
        .south_west = {min_lat, min_lon},
        .north_east = {max_lat, max_lon},
    };
}

}  // namespace garraiobide::core::domain
