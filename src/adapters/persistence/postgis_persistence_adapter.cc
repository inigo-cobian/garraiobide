#include "postgis_persistence_adapter.h"

#include "../../core/domain/entity_conversion.h"

namespace garraiobide::adapters::persistence {

using core::domain::BoundingBox;
using core::domain::GeoFeature;
using core::domain::Layer;
using core::domain::SpatialScale;
using core::ports::PersistenceError;

PostgisPersistenceAdapter::PostgisPersistenceAdapter(
    core::ports::TransitRepositoryPort& transit_repo)
    : transit_repo_(transit_repo) {}

PostgisPersistenceAdapter::LayerType
PostgisPersistenceAdapter::classify_layer_name(const std::string& name) {
    // Match exact names or suffix patterns like "xxx_routes"
    if (name == "routes" || name.ends_with("_routes")) return LayerType::Routes;
    if (name == "stops" || name.ends_with("_stops")) return LayerType::Stops;
    if (name == "entrances" || name.ends_with("_entrances")) return LayerType::Entrances;
    return LayerType::Unknown;
}

std::expected<void, PersistenceError>
PostgisPersistenceAdapter::save_layer(const Layer& layer) {
    // PostGIS adapter is primarily read-focused for the map API.
    // Writing is handled by the TransitRepository directly via typed entities.
    // For compatibility, we accept saves but ignore them (data should be
    // ingested via the typed path).
    (void)layer;
    return {};
}

std::expected<Layer, PersistenceError>
PostgisPersistenceAdapter::find_layer(const std::string& name) {
    auto type = classify_layer_name(name);

    switch (type) {
        case LayerType::Routes: {
            auto routes = transit_repo_.list_routes("");
            if (!routes) return std::unexpected(PersistenceError::ConnectionError);
            return core::domain::routes_to_layer(*routes, name);
        }
        case LayerType::Stops: {
            auto stops = transit_repo_.list_stops("");
            if (!stops) return std::unexpected(PersistenceError::ConnectionError);
            return core::domain::stops_to_layer(*stops, name);
        }
        case LayerType::Entrances: {
            // List all entrances across all stops
            // Since list_entrances requires a stop_id, we query all stops first
            auto stops = transit_repo_.list_stops("");
            if (!stops) return std::unexpected(PersistenceError::ConnectionError);

            std::vector<core::domain::Entrance> all_entrances;
            for (const auto& stop : *stops) {
                auto entrances = transit_repo_.list_entrances(stop.id);
                if (entrances) {
                    all_entrances.insert(all_entrances.end(),
                        entrances->begin(), entrances->end());
                }
            }
            return core::domain::entrances_to_layer(all_entrances, name);
        }
        case LayerType::Unknown:
            return std::unexpected(PersistenceError::NotFound);
    }

    return std::unexpected(PersistenceError::NotFound);
}

std::expected<std::vector<std::string>, PersistenceError>
PostgisPersistenceAdapter::list_layers() {
    // Synthesize layer names from what's available in PostGIS.
    // We always report routes and stops as available; entrances only if any exist.
    std::vector<std::string> layers;

    auto routes = transit_repo_.list_routes("");
    if (routes && !routes->empty()) {
        // Derive prefix from first route's agency_id
        std::string prefix;
        auto agencies = transit_repo_.list_agencies();
        if (agencies && !agencies->empty()) {
            prefix = agencies->at(0).id;
        }
        if (prefix.empty()) prefix = "transit";

        layers.push_back(prefix + "_routes");
        layers.push_back(prefix + "_stops");
    } else {
        // Check if stops exist even without routes
        auto stops = transit_repo_.list_stops("");
        if (stops && !stops->empty()) {
            layers.push_back("stops");
        }
    }

    return layers;
}

std::expected<void, PersistenceError>
PostgisPersistenceAdapter::remove_layer(const std::string& name) {
    // Removing typed entities via the layer interface is not supported.
    // Use the TransitRepository directly for entity management.
    (void)name;
    return std::unexpected(PersistenceError::WriteError);
}

std::expected<std::vector<GeoFeature>, PersistenceError>
PostgisPersistenceAdapter::query_features(const BoundingBox& extent) {
    // Query stops within the bounding box (most common spatial query for maps)
    auto stops = transit_repo_.query_stops(extent);
    if (!stops) return std::unexpected(PersistenceError::ConnectionError);

    std::vector<GeoFeature> features;
    features.reserve(stops->size());
    for (const auto& stop : *stops) {
        features.push_back(core::domain::stop_to_feature(stop));
    }

    // Also include routes whose geometry intersects the bbox.
    // Since TransitRepository doesn't have a spatial route query,
    // we include all routes (they're typically few enough for the API).
    auto routes = transit_repo_.list_routes("");
    if (routes) {
        for (const auto& route : *routes) {
            features.push_back(core::domain::route_to_feature(route));
        }
    }

    return features;
}

}  // namespace garraiobide::adapters::persistence
