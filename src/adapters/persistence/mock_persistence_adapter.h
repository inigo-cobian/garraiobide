#pragma once

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/core/domain/layer.h"
#include "src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {

/// In-memory mock persistence adapter.
/// Stores layers in a hash map — suitable for testing without a real database.
class MockPersistenceAdapter final : public core::ports::PersistencePort {
   public:
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

    /// Direct access to storage for test assertions.
    [[nodiscard]] std::size_t layer_count() const { return store_.size(); }

   private:
    std::unordered_map<std::string, core::domain::Layer> store_;
};

}  // namespace garraiobide::adapters::persistence
