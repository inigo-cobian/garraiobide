#pragma once

#include <expected>
#include <string>
#include <vector>

#include "src/core/domain/bounding_box.h"
#include "src/core/domain/layer.h"
#include "src/core/ports/data_ingestion_port.h"
#include "src/core/ports/persistence_port.h"
#include "src/core/ports/presentation_port.h"

namespace garraiobide::app {

/// Application-level errors surfaced to driving adapters.
enum class LayerServiceError {
    IngestionFailed,
    PersistenceFailed,
    LayerNotFound,
    DuplicateLayer,
};

/// Application service that orchestrates layer-related use cases.
/// Depends only on port interfaces — never on concrete adapters.
class LayerService {
   public:
    LayerService(core::ports::DataIngestionPort& ingestion,
                 core::ports::PersistencePort& persistence,
                 core::ports::PresentationPort& presentation);

    /// Ingest features from a source, wrap them in a layer, persist and present.
    [[nodiscard]] std::expected<void, LayerServiceError>
    import_layer(const std::string& name,
                 const std::string& source,
                 core::domain::SpatialScale scale);

    /// List all persisted layer names and present them.
    [[nodiscard]] std::expected<std::vector<std::string>, LayerServiceError>
    list_layers();

    /// Load a layer from persistence and present it.
    [[nodiscard]] std::expected<core::domain::Layer, LayerServiceError>
    show_layer(const std::string& name);

    /// Remove a layer from persistence.
    [[nodiscard]] std::expected<void, LayerServiceError>
    remove_layer(const std::string& name);

    /// Query features within an extent across all layers.
    [[nodiscard]] std::expected<std::vector<core::domain::GeoFeature>, LayerServiceError>
    query_features(const core::domain::BoundingBox& extent);

   private:
    core::ports::DataIngestionPort& ingestion_;
    core::ports::PersistencePort& persistence_;
    core::ports::PresentationPort& presentation_;
};

}  // namespace garraiobide::app
