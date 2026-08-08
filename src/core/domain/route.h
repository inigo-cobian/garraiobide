#pragma once

#include <optional>
#include <string>
#include <vector>

#include "geometry.h"

namespace garraiobide::core::domain {

/// A station entry within a route's ordered station sequence.
struct StationEntry {
    std::string id;
    std::string name;
    int child_count{0};
};

/// A transit route belonging to an agency.
struct Route {
    std::string id;
    std::string agency_id;
    std::optional<std::string> short_name;
    std::optional<std::string> long_name;
    int route_type{0};  // GTFS route_type (0=tram, 1=subway, 2=rail, 3=bus, etc.)
    std::optional<std::string> color;
    std::optional<std::string> text_color;
    std::optional<Geometry> geometry;
    std::vector<StationEntry> station_sequence;
};

}  // namespace garraiobide::core::domain
