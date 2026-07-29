#pragma once

#include <string>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include "../../core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {

/// MongoDB-backed persistence adapter. Stores layers as BSON documents
/// with GeoJSON geometry for native 2dsphere spatial queries.
class MongoPersistenceAdapter final : public core::ports::PersistencePort {
   public:
    /// Construct with MongoDB connection URI and database name.
    /// Ensures mongocxx::instance is initialized (once per process).
    /// Creates indexes on first use.
    MongoPersistenceAdapter(std::string connection_string, std::string database_name);

    ~MongoPersistenceAdapter() override = default;

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
    std::string connection_string_;
    std::string database_name_;
    mongocxx::client client_;
    mongocxx::database db_;
    mongocxx::collection layers_collection_;

    /// Ensure required indexes exist (unique name + 2dsphere on features.geometry).
    [[nodiscard]] std::expected<void, core::ports::PersistenceError> ensure_indexes();
};

}  // namespace garraiobide::adapters::persistence
