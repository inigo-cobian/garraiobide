#pragma once

#include <expected>
#include <string>
#include <vector>

#include "src/core/domain/bounding_box.h"
#include "src/core/domain/geo_feature.h"

namespace garraiobide::core::ports {

/// Errors that data ingestion adapters may report.
enum class IngestionError {
    SourceNotFound,
    ParseError,
    UnsupportedFormat,
    NetworkError,
};

/// Driven port: adapters that load geospatial features from external sources
/// (GeoJSON files, WCS services, OSM API, etc.).
class DataIngestionPort {
   public:
    virtual ~DataIngestionPort() = default;

    /// Load all features from the given source identifier.
    /// The source string is adapter-specific (file path, URL, query, etc.).
    [[nodiscard]] virtual std::expected<std::vector<domain::GeoFeature>, IngestionError>
    load_features(const std::string& source) = 0;

    /// Load features within a spatial extent.
    [[nodiscard]] virtual std::expected<std::vector<domain::GeoFeature>, IngestionError>
    load_features_within(const std::string& source, const domain::BoundingBox& extent) = 0;
};

}  // namespace garraiobide::core::ports
