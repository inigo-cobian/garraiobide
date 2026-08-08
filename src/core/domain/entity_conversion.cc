#include "entity_conversion.h"

#include <nlohmann/json.hpp>

namespace garraiobide::core::domain {

namespace {

std::string stop_type_to_string(StopType type) {
    switch (type) {
        case StopType::ParentStation:
            return "parent_station";
        case StopType::ChildStop:
            return "child_stop";
        case StopType::Standalone:
            return "standalone";
    }
    return "standalone";
}

}  // namespace

GeoFeature stop_to_feature(const Stop& stop) {
    GeoFeature feature;
    feature.id = stop.id;
    feature.geometry = Point{.position = stop.position};

    feature.properties["stop_type"] = stop_type_to_string(stop.stop_type);
    feature.properties["stop_name"] = stop.name;

    if (stop.code.has_value()) {
        feature.properties["stop_code"] = stop.code.value();
    }
    if (stop.url.has_value()) {
        feature.properties["stop_url"] = stop.url.value();
    }
    if (stop.parent_stop_id.has_value()) {
        feature.properties["parent_station"] = stop.parent_stop_id.value();
    }

    // Serialize route_ids as a JSON array string (matches existing format)
    nlohmann::json route_ids_arr = nlohmann::json::array();
    for (const auto& rid : stop.route_ids) {
        route_ids_arr.push_back(rid);
    }
    feature.properties["route_ids"] = route_ids_arr.dump();

    return feature;
}

GeoFeature route_to_feature(const Route& route) {
    GeoFeature feature;
    feature.id = route.id;

    if (route.geometry.has_value()) {
        feature.geometry = route.geometry.value();
    } else {
        // Default to empty LineString if no geometry
        feature.geometry = LineString{};
    }

    if (route.short_name.has_value()) {
        feature.properties["route_short_name"] = route.short_name.value();
    }
    if (route.long_name.has_value()) {
        feature.properties["route_long_name"] = route.long_name.value();
    }
    feature.properties["route_type"] = static_cast<int64_t>(route.route_type);
    if (route.color.has_value()) {
        feature.properties["route_color"] = route.color.value();
    }
    if (route.text_color.has_value()) {
        feature.properties["route_text_color"] = route.text_color.value();
    }

    // Serialize station_sequence as JSON array string (matches existing format)
    if (!route.station_sequence.empty()) {
        nlohmann::json seq_arr = nlohmann::json::array();
        for (const auto& entry : route.station_sequence) {
            nlohmann::json obj;
            obj["id"] = entry.id;
            obj["name"] = entry.name;
            obj["child_count"] = entry.child_count;
            seq_arr.push_back(obj);
        }
        feature.properties["station_sequence"] = seq_arr.dump();
    }

    return feature;
}

GeoFeature entrance_to_feature(const Entrance& entrance) {
    GeoFeature feature;
    feature.id = entrance.id;
    feature.geometry = Point{.position = entrance.position};

    feature.properties["stop_id"] = entrance.stop_id;
    if (entrance.name.has_value()) {
        feature.properties["entrance_name"] = entrance.name.value();
    }

    return feature;
}

Layer stops_to_layer(const std::vector<Stop>& stops, const std::string& layer_name) {
    Layer layer;
    layer.name = layer_name;
    layer.scale = SpatialScale::Urban;
    layer.features.reserve(stops.size());

    for (const auto& stop : stops) {
        layer.features.push_back(stop_to_feature(stop));
    }

    return layer;
}

Layer routes_to_layer(const std::vector<Route>& routes, const std::string& layer_name) {
    Layer layer;
    layer.name = layer_name;
    layer.scale = SpatialScale::Urban;
    layer.features.reserve(routes.size());

    for (const auto& route : routes) {
        layer.features.push_back(route_to_feature(route));
    }

    return layer;
}

Layer entrances_to_layer(const std::vector<Entrance>& entrances,
                         const std::string& layer_name) {
    Layer layer;
    layer.name = layer_name;
    layer.scale = SpatialScale::Urban;
    layer.features.reserve(entrances.size());

    for (const auto& entrance : entrances) {
        layer.features.push_back(entrance_to_feature(entrance));
    }

    return layer;
}

}  // namespace garraiobide::core::domain
