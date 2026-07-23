#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "core/ports/data_ingestion_port.h"


namespace garraiobide::adapters::ingestion::gtfs {

/// GTFS ingestion adapter — reads GTFS ZIP archives and produces GeoFeatures.
/// Implements DataIngestionPort for integration with the hexagonal architecture.
class GtfsIngestionAdapter final : public core::ports::DataIngestionPort {
   public:
    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>,
                                core::ports::IngestionError>
    load_features(const std::string& source) override;

    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>,
                                core::ports::IngestionError>
    load_features_within(const std::string& source,
                         const core::domain::BoundingBox& extent) override;

   private:
    /// Read a file from a ZIP archive. Returns file contents or error.
    [[nodiscard]] std::expected<std::string, core::ports::IngestionError>
    read_zip_entry(const std::string& zip_path, const std::string& entry_name);

    /// Check if a ZIP archive contains a given entry.
    [[nodiscard]] bool zip_contains(const std::string& zip_path,
                                    const std::string& entry_name);
};

}  // namespace garraiobide::adapters::ingestion::gtfs
