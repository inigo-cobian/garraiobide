#include "src/adapters/ingestion/mock_ingestion_adapter.h"

#include <algorithm>

namespace garraiobide::adapters::ingestion {

void MockIngestionAdapter::set_features(
    std::vector<core::domain::GeoFeature> features) {
    features_ = std::move(features);
}

void MockIngestionAdapter::set_error(core::ports::IngestionError error) {
    error_ = error;
}

void MockIngestionAdapter::clear_error() { error_.reset(); }

std::expected<std::vector<core::domain::GeoFeature>, core::ports::IngestionError>
MockIngestionAdapter::load_features(const std::string& source) {
    last_source_ = source;
    if (error_) {
        return std::unexpected(*error_);
    }
    return features_;
}

std::expected<std::vector<core::domain::GeoFeature>, core::ports::IngestionError>
MockIngestionAdapter::load_features_within(
    const std::string& source, const core::domain::BoundingBox& extent) {
    last_source_ = source;
    if (error_) {
        return std::unexpected(*error_);
    }

    // Simple filtering: keep only Point features within the extent.
    std::vector<core::domain::GeoFeature> result;
    for (const auto& feature : features_) {
        if (auto* point =
                std::get_if<core::domain::Point>(&feature.geometry)) {
            if (extent.contains(point->position)) {
                result.push_back(feature);
            }
        } else {
            // Non-point features are included unconditionally in this mock.
            result.push_back(feature);
        }
    }
    return result;
}

}  // namespace garraiobide::adapters::ingestion
