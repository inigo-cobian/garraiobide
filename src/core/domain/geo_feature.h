#pragma once

#include <optional>
#include <string>

#include "src/core/domain/geometry.h"
#include "src/core/domain/properties.h"

namespace garraiobide::core::domain {

/// A geographic feature: geometry + attributes, optionally identified.
struct GeoFeature {
    std::optional<std::string> id;
    Geometry geometry;
    Properties properties;
};

}  // namespace garraiobide::core::domain
