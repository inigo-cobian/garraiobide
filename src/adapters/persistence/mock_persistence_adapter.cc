#include "mock_persistence_adapter.h"

namespace garraiobide::adapters::persistence {

std::expected<void, core::ports::PersistenceError>
MockPersistenceAdapter::save_layer(const core::domain::Layer& layer) {
    auto [_, inserted] = store_.try_emplace(layer.name, layer);
    if (!inserted) {
        return std::unexpected(core::ports::PersistenceError::DuplicateLayer);
    }
    return {};
}

std::expected<core::domain::Layer, core::ports::PersistenceError>
MockPersistenceAdapter::find_layer(const std::string& name) {
    auto it = store_.find(name);
    if (it == store_.end()) {
        return std::unexpected(core::ports::PersistenceError::NotFound);
    }
    return it->second;
}

std::expected<std::vector<std::string>, core::ports::PersistenceError>
MockPersistenceAdapter::list_layers() {
    std::vector<std::string> names;
    names.reserve(store_.size());
    for (const auto& [name, _] : store_) {
        names.push_back(name);
    }
    return names;
}

std::expected<void, core::ports::PersistenceError>
MockPersistenceAdapter::remove_layer(const std::string& name) {
    auto it = store_.find(name);
    if (it == store_.end()) {
        return std::unexpected(core::ports::PersistenceError::NotFound);
    }
    store_.erase(it);
    return {};
}

std::expected<std::vector<core::domain::GeoFeature>, core::ports::PersistenceError>
MockPersistenceAdapter::query_features(const core::domain::BoundingBox& extent) {
    std::vector<core::domain::GeoFeature> result;
    for (const auto& [_, layer] : store_) {
        for (const auto& feature : layer.features) {
            if (auto* point =
                    std::get_if<core::domain::Point>(&feature.geometry)) {
                if (extent.contains(point->position)) {
                    result.push_back(feature);
                }
            }
        }
    }
    return result;
}

}  // namespace garraiobide::adapters::persistence
