#include "geojson_serializer.h"

#include <nlohmann/json.hpp>
#include <variant>

namespace garraiobide::adapters::http {

namespace {

using nlohmann::json;
using namespace garraiobide::core::domain;

/// Convert a Coordinate to a GeoJSON position array [longitude, latitude].
json coordinate_to_json(const Coordinate& coord) {
    return json::array({coord.longitude, coord.latitude});
}

/// Convert a Geometry variant to a GeoJSON geometry object.
json geometry_to_json(const Geometry& geometry) {
    return std::visit(
        [](const auto& geo) -> json {
            using T = std::decay_t<decltype(geo)>;

            if constexpr (std::is_same_v<T, Point>) {
                return {{"type", "Point"},
                        {"coordinates", coordinate_to_json(geo.position)}};
            } else if constexpr (std::is_same_v<T, LineString>) {
                json coords = json::array();
                for (const auto& vertex : geo.vertices) {
                    coords.push_back(coordinate_to_json(vertex));
                }
                return {{"type", "LineString"}, {"coordinates", coords}};
            } else if constexpr (std::is_same_v<T, Polygon>) {
                json rings = json::array();
                for (const auto& ring : geo.rings) {
                    json ring_coords = json::array();
                    for (const auto& vertex : ring) {
                        ring_coords.push_back(coordinate_to_json(vertex));
                    }
                    rings.push_back(ring_coords);
                }
                return {{"type", "Polygon"}, {"coordinates", rings}};
            }
        },
        geometry);
}

/// Convert a PropertyValue variant to a JSON value.
json property_value_to_json(const PropertyValue& value) {
    return std::visit(
        [](const auto& v) -> json { return json(v); }, value);
}

/// Convert a Properties map to a JSON object.
json properties_to_json(const Properties& properties) {
    json obj = json::object();
    for (const auto& [key, value] : properties) {
        obj[key] = property_value_to_json(value);
    }
    return obj;
}

/// Build a GeoJSON Feature object from a GeoFeature.
json feature_to_json(const GeoFeature& feature) {
    json obj = json::object();
    obj["type"] = "Feature";

    if (feature.id.has_value()) {
        obj["id"] = feature.id.value();
    }

    obj["geometry"] = geometry_to_json(feature.geometry);
    obj["properties"] = properties_to_json(feature.properties);

    return obj;
}

/// Build a GeoJSON FeatureCollection from a vector of features.
json feature_collection_to_json(
    const std::vector<GeoFeature>& features) {
    json collection = json::object();
    collection["type"] = "FeatureCollection";

    json features_array = json::array();
    for (const auto& feature : features) {
        features_array.push_back(feature_to_json(feature));
    }
    collection["features"] = features_array;

    return collection;
}

}  // namespace

std::string GeoJsonSerializer::serialize_feature(
    const core::domain::GeoFeature& feature) {
    return feature_to_json(feature).dump();
}

std::string GeoJsonSerializer::serialize_layer(
    const core::domain::Layer& layer) {
    return feature_collection_to_json(layer.features).dump();
}

std::string GeoJsonSerializer::serialize_feature_collection(
    const std::vector<core::domain::GeoFeature>& features) {
    return feature_collection_to_json(features).dump();
}

}  // namespace garraiobide::adapters::http
