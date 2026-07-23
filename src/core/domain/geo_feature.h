#pragma once

#include <optional>
#include <string>

#include "geometry.h"
#include "properties.h"

namespace garraiobide::core::domain {

/// A geographic feature: geometry + attributes, optionally identified.
struct GeoFeature {
    std::optional<std::string> id;
    Geometry geometry;
    Properties properties;
};

}  // namespace garraiobide::core::domain
