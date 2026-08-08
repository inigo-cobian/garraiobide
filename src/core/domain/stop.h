#pragma once

#include <optional>
#include <string>
#include <vector>

#include "coordinate.h"

namespace garraiobide::core::domain {

/// Classification of a stop within a station hierarchy.
enum class StopType {
    ParentStation,
    ChildStop,
    Standalone,
};

/// A transit stop or station with a geographic position.
struct Stop {
    std::string id;
    std::string name;
    std::optional<std::string> code;
    std::optional<std::string> url;
    Coordinate position;
    StopType stop_type{StopType::Standalone};
    std::optional<std::string> parent_stop_id;
    std::vector<std::string> route_ids;  // Routes serving this stop
};

}  // namespace garraiobide::core::domain
