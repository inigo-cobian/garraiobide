#pragma once

#include <filesystem>
#include <string>

#include "src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {

/// File-based persistence adapter. Stores each layer as a JSON file.
class FilePersistenceAdapter final : public core::ports::PersistencePort {
   public:
    /// Construct with a path to the data directory.
    explicit FilePersistenceAdapter(std::filesystem::path data_dir);

    [[nodiscard]] std::expected<void, core::ports::PersistenceError>
    save_layer(const core::domain::Layer& layer) override;

    [[nodiscard]] std::expected<core::domain::Layer, core::ports::PersistenceError>
    find_layer(const std::string& name) override;

    [[nodiscard]] std::expected<std::vector<std::string>, core::ports::PersistenceError>
    list_layers() override;

    [[nodiscard]] std::expected<void, core::ports::PersistenceError>
    remove_layer(const std::string& name) override;

    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>,
                                core::ports::PersistenceError>
    query_features(const core::domain::BoundingBox& extent) override;

   private:
    std::filesystem::path data_dir_;

    /// Get the file path for a given layer name.
    [[nodiscard]] std::filesystem::path layer_path(const std::string& name) const;

    /// Create the data directory if it does not exist.
    void ensure_directory_exists();
};

}  // namespace garraiobide::adapters::persistence
