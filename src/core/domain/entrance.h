#pragma once

#include <optional>
#include <string>

#include "coordinate.h"

namespace garraiobide::core::domain {

/// A physical entrance to a stop or station.
struct Entrance {
    std::string id;
    std::string stop_id;
    std::optional<std::string> name;
    Coordinate position;
};

}  // namespace garraiobide::core::domain
