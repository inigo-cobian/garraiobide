#pragma once

#include <string>
#include <vector>

#include "src/core/domain/geo_feature.h"
#include "src/core/domain/layer.h"

namespace garraiobide::adapters::http {

/// Serializes domain objects to RFC 7946 GeoJSON strings.
class GeoJsonSerializer {
   public:
    /// Serialize a single GeoFeature to a GeoJSON Feature JSON string.
    [[nodiscard]] static std::string serialize_feature(
        const core::domain::GeoFeature& feature);

    /// Serialize a Layer to a GeoJSON FeatureCollection JSON string.
    [[nodiscard]] static std::string serialize_layer(
        const core::domain::Layer& layer);

    /// Serialize a vector of features to a GeoJSON FeatureCollection JSON string.
    [[nodiscard]] static std::string serialize_feature_collection(
        const std::vector<core::domain::GeoFeature>& features);
};

}  // namespace garraiobide::adapters::http
