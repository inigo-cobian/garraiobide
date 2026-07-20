#pragma once

#include <string>
#include <vector>

#include "src/core/domain/bounding_box.h"
#include "src/core/domain/geo_feature.h"

namespace garraiobide::core::domain {

/// Spatial scale at which data is presented.
enum class SpatialScale {
    Urban,     // City / neighbourhood level
    Regional,  // Region / metropolitan area
};

/// A named collection of geographic features at a given spatial scale.
struct Layer {
    std::string name;
    SpatialScale scale{SpatialScale::Urban};
    std::vector<GeoFeature> features;

    /// Compute the bounding box that encloses all point features.
    /// Returns nullopt if the layer has no point geometries.
    [[nodiscard]] std::optional<BoundingBox> envelope() const;
};

}  // namespace garraiobide::core::domain
