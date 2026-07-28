#include "layer_service.h"

namespace garraiobide::app {

LayerService::LayerService(core::ports::DataIngestionPort& ingestion,
                           core::ports::PersistencePort& persistence,
                           core::ports::PresentationPort& presentation)
    : ingestion_(ingestion),
      persistence_(persistence),
      presentation_(presentation) {}

std::expected<void, LayerServiceError>
LayerService::import_layer(const std::string& name,
                           const std::string& source,
                           core::domain::SpatialScale scale) {
    auto features_result = ingestion_.load_features(source);
    if (!features_result) {
        presentation_.show_error("Failed to ingest data from: " + source);
        return std::unexpected(LayerServiceError::IngestionFailed);
    }

    core::domain::Layer layer{
        .name = name,
        .scale = scale,
        .features = std::move(*features_result),
    };

    auto save_result = persistence_.save_layer(layer);
    if (!save_result) {
        if (save_result.error() == core::ports::PersistenceError::DuplicateLayer) {
            presentation_.show_error("Layer already exists: " + name);
            return std::unexpected(LayerServiceError::DuplicateLayer);
        }
        presentation_.show_error("Failed to persist layer: " + name);
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    presentation_.render_layer(layer);
    presentation_.show_message("Layer imported: " + name);
    return {};
}

std::expected<std::vector<std::string>, LayerServiceError>
LayerService::list_layers() {
    auto result = persistence_.list_layers();
    if (!result) {
        presentation_.show_error("Failed to list layers");
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    presentation_.present_layer_list(*result);
    return *result;
}

std::expected<core::domain::Layer, LayerServiceError>
LayerService::show_layer(const std::string& name) {
    auto result = persistence_.find_layer(name);
    if (!result) {
        if (result.error() == core::ports::PersistenceError::NotFound) {
            presentation_.show_error("Layer not found: " + name);
            return std::unexpected(LayerServiceError::LayerNotFound);
        }
        presentation_.show_error("Failed to load layer: " + name);
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    presentation_.render_layer(*result);
    return *result;
}

std::expected<void, LayerServiceError>
LayerService::remove_layer(const std::string& name) {
    auto result = persistence_.remove_layer(name);
    if (!result) {
        if (result.error() == core::ports::PersistenceError::NotFound) {
            presentation_.show_error("Layer not found: " + name);
            return std::unexpected(LayerServiceError::LayerNotFound);
        }
        presentation_.show_error("Failed to remove layer: " + name);
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    presentation_.show_message("Layer removed: " + name);
    return {};
}

std::expected<std::vector<core::domain::GeoFeature>, LayerServiceError>
LayerService::query_features(const core::domain::BoundingBox& extent) {
    auto result = persistence_.query_features(extent);
    if (!result) {
        presentation_.show_error("Failed to query features");
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    return *result;
}

std::expected<std::vector<std::string>, LayerServiceError>
LayerService::import_gtfs(const std::string& source,
                          const std::string& layer_prefix) {
    auto features_result = ingestion_.load_features(source);
    if (!features_result) {
        presentation_.show_error("Failed to ingest GTFS data from: " + source);
        return std::unexpected(LayerServiceError::IngestionFailed);
    }

    // Partition features by geometry type: Point → stops, LineString → routes
    std::vector<core::domain::GeoFeature> route_features;
    std::vector<core::domain::GeoFeature> stop_features;

    for (auto& feature : *features_result) {
        if (std::holds_alternative<core::domain::Point>(feature.geometry)) {
            stop_features.push_back(std::move(feature));
        } else if (std::holds_alternative<core::domain::LineString>(feature.geometry)) {
            route_features.push_back(std::move(feature));
        }
    }

    std::string routes_name = layer_prefix + "_routes";
    std::string stops_name = layer_prefix + "_stops";

    // Persist routes layer
    core::domain::Layer routes_layer{
        .name = routes_name,
        .scale = core::domain::SpatialScale::Urban,
        .features = std::move(route_features),
    };

    auto routes_save = persistence_.save_layer(routes_layer);
    if (!routes_save) {
        if (routes_save.error() == core::ports::PersistenceError::DuplicateLayer) {
            presentation_.show_error("Layer already exists: " + routes_name);
            return std::unexpected(LayerServiceError::DuplicateLayer);
        }
        presentation_.show_error("Failed to persist layer: " + routes_name);
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    // Persist stops layer
    core::domain::Layer stops_layer{
        .name = stops_name,
        .scale = core::domain::SpatialScale::Urban,
        .features = std::move(stop_features),
    };

    auto stops_save = persistence_.save_layer(stops_layer);
    if (!stops_save) {
        if (stops_save.error() == core::ports::PersistenceError::DuplicateLayer) {
            presentation_.show_error("Layer already exists: " + stops_name);
            return std::unexpected(LayerServiceError::DuplicateLayer);
        }
        presentation_.show_error("Failed to persist layer: " + stops_name);
        return std::unexpected(LayerServiceError::PersistenceFailed);
    }

    return std::vector<std::string>{routes_name, stops_name};
}

}  // namespace garraiobide::app
