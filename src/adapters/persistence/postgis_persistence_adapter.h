#pragma once

#include <memory>
#include <string>

#include "../../core/ports/persistence_port.h"
#include "../../core/ports/transit_repository_port.h"

namespace garraiobide::adapters::persistence {

/// PersistencePort adapter that reads from PostGIS via TransitRepositoryPort.
/// Synthesizes virtual layers from typed entities:
///   - "routes"    → all routes converted to GeoFeatures
///   - "stops"     → all stops converted to GeoFeatures
///   - "entrances" → all entrances converted to GeoFeatures
///
/// Also recognizes agency-prefixed layer names (e.g. "metro_bilbao_routes").
///
/// save_layer delegates to the transit repository by parsing features back
/// into typed entities. remove_layer is not supported (returns WriteError).
class PostgisPersistenceAdapter final : public core::ports::PersistencePort {
   public:
    /// Construct with a reference to a TransitRepositoryPort implementation.
    explicit PostgisPersistenceAdapter(core::ports::TransitRepositoryPort& transit_repo);

    ~PostgisPersistenceAdapter() override = default;

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
    core::ports::TransitRepositoryPort& transit_repo_;

    /// Check if a layer name corresponds to routes, stops, or entrances.
    enum class LayerType { Routes, Stops, Entrances, Unknown };
    [[nodiscard]] static LayerType classify_layer_name(const std::string& name);
};

}  // namespace garraiobide::adapters::persistence
