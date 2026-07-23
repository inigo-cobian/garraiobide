#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "../domain/bounding_box.h"
#include "../domain/layer.h"

namespace garraiobide::core::ports {

/// Errors that persistence adapters may report.
enum class PersistenceError {
    NotFound,
    WriteError,
    ConnectionError,
    DuplicateLayer,
};

/// Driven port: adapters that persist and retrieve layers/features
/// (PostGIS, SQLite/SpatiaLite, file-based stores, etc.).
class PersistencePort {
   public:
    virtual ~PersistencePort() = default;

    /// Store a layer. Fails if a layer with the same name already exists.
    [[nodiscard]] virtual std::expected<void, PersistenceError>
    save_layer(const domain::Layer& layer) = 0;

    /// Retrieve a layer by name.
    [[nodiscard]] virtual std::expected<domain::Layer, PersistenceError>
    find_layer(const std::string& name) = 0;

    /// List all stored layer names.
    [[nodiscard]] virtual std::expected<std::vector<std::string>, PersistenceError>
    list_layers() = 0;

    /// Remove a layer by name.
    [[nodiscard]] virtual std::expected<void, PersistenceError>
    remove_layer(const std::string& name) = 0;

    /// Query features within a bounding box across all layers.
    [[nodiscard]] virtual std::expected<std::vector<domain::GeoFeature>, PersistenceError>
    query_features(const domain::BoundingBox& extent) = 0;
};

}  // namespace garraiobide::core::ports
