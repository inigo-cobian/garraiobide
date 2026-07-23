#pragma once

#include <expected>
#include <string>
#include <vector>

#include "../../core/domain/geo_feature.h"
#include "../../core/ports/data_ingestion_port.h"

namespace garraiobide::adapters::ingestion {

/// Mock ingestion adapter that returns pre-configured features.
/// Useful for testing and development without real data sources.
class MockIngestionAdapter final : public core::ports::DataIngestionPort {
   public:
    /// Configure the features this mock will return on next load call.
    void set_features(std::vector<core::domain::GeoFeature> features);

    /// Configure the mock to return an error on next load call.
    void set_error(core::ports::IngestionError error);

    /// Clear any configured error so loads succeed again.
    void clear_error();

    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>,
                                core::ports::IngestionError>
    load_features(const std::string& source) override;

    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>,
                                core::ports::IngestionError>
    load_features_within(const std::string& source,
                         const core::domain::BoundingBox& extent) override;

    /// Inspect what source was last requested.
    [[nodiscard]] const std::string& last_source() const { return last_source_; }

   private:
    std::vector<core::domain::GeoFeature> features_;
    std::optional<core::ports::IngestionError> error_;
    std::string last_source_;
};

}  // namespace garraiobide::adapters::ingestion
