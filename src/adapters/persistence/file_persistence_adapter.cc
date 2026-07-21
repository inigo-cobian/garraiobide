#include "src/adapters/persistence/file_persistence_adapter.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace garraiobide::adapters::persistence {

using json = nlohmann::json;
using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Geometry;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::Properties;
using core::domain::PropertyValue;
using core::domain::SpatialScale;
using core::ports::PersistenceError;

namespace {

// --- Serialization helpers ---

std::string scale_to_string(SpatialScale scale) {
    switch (scale) {
        case SpatialScale::Urban:
            return "Urban";
        case SpatialScale::Regional:
            return "Regional";
    }
    return "Urban";
}

SpatialScale string_to_scale(const std::string& s) {
    if (s == "Regional") return SpatialScale::Regional;
    return SpatialScale::Urban;
}

json coordinate_to_json(const Coordinate& coord) {
    // Store in domain order: [latitude, longitude]
    return json::array({coord.latitude, coord.longitude});
}

Coordinate json_to_coordinate(const json& j) {
    return Coordinate{.latitude = j[0].get<double>(),
                      .longitude = j[1].get<double>()};
}

json geometry_to_json(const Geometry& geom) {
    json j;
    std::visit(
        [&j](const auto& g) {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, Point>) {
                j["type"] = "Point";
                j["coordinates"] = coordinate_to_json(g.position);
            } else if constexpr (std::is_same_v<T, LineString>) {
                j["type"] = "LineString";
                json coords = json::array();
                for (const auto& v : g.vertices) {
                    coords.push_back(coordinate_to_json(v));
                }
                j["coordinates"] = std::move(coords);
            } else if constexpr (std::is_same_v<T, Polygon>) {
                j["type"] = "Polygon";
                json rings = json::array();
                for (const auto& ring : g.rings) {
                    json ring_coords = json::array();
                    for (const auto& v : ring) {
                        ring_coords.push_back(coordinate_to_json(v));
                    }
                    rings.push_back(std::move(ring_coords));
                }
                j["coordinates"] = std::move(rings);
            }
        },
        geom);
    return j;
}

Geometry json_to_geometry(const json& j) {
    const auto& type = j.at("type").get<std::string>();
    if (type == "Point") {
        return Point{.position = json_to_coordinate(j.at("coordinates"))};
    }
    if (type == "LineString") {
        std::vector<Coordinate> vertices;
        for (const auto& coord : j.at("coordinates")) {
            vertices.push_back(json_to_coordinate(coord));
        }
        return LineString{.vertices = std::move(vertices)};
    }
    // Polygon
    std::vector<std::vector<Coordinate>> rings;
    for (const auto& ring : j.at("coordinates")) {
        std::vector<Coordinate> ring_coords;
        for (const auto& coord : ring) {
            ring_coords.push_back(json_to_coordinate(coord));
        }
        rings.push_back(std::move(ring_coords));
    }
    return Polygon{.rings = std::move(rings)};
}

json property_value_to_json(const PropertyValue& val) {
    return std::visit(
        [](const auto& v) -> json { return json(v); }, val);
}

PropertyValue json_to_property_value(const json& j) {
    if (j.is_string()) return j.get<std::string>();
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer()) return j.get<int64_t>();
    if (j.is_number_float()) return j.get<double>();
    // Fallback — treat as string
    return j.dump();
}

json properties_to_json(const Properties& props) {
    json j = json::object();
    for (const auto& [key, val] : props) {
        j[key] = property_value_to_json(val);
    }
    return j;
}

Properties json_to_properties(const json& j) {
    Properties props;
    for (auto it = j.begin(); it != j.end(); ++it) {
        props[it.key()] = json_to_property_value(it.value());
    }
    return props;
}

json feature_to_json(const GeoFeature& feature) {
    json j;
    if (feature.id.has_value()) {
        j["id"] = feature.id.value();
    }
    j["geometry"] = geometry_to_json(feature.geometry);
    j["properties"] = properties_to_json(feature.properties);
    return j;
}

GeoFeature json_to_feature(const json& j) {
    GeoFeature feature;
    if (j.contains("id") && !j["id"].is_null()) {
        feature.id = j["id"].get<std::string>();
    }
    feature.geometry = json_to_geometry(j.at("geometry"));
    if (j.contains("properties") && !j["properties"].is_null()) {
        feature.properties = json_to_properties(j.at("properties"));
    }
    return feature;
}

json layer_to_json(const Layer& layer) {
    json j;
    j["name"] = layer.name;
    j["scale"] = scale_to_string(layer.scale);
    json features = json::array();
    for (const auto& f : layer.features) {
        features.push_back(feature_to_json(f));
    }
    j["features"] = std::move(features);
    return j;
}

Layer json_to_layer(const json& j) {
    Layer layer;
    layer.name = j.at("name").get<std::string>();
    if (j.contains("scale")) {
        layer.scale = string_to_scale(j.at("scale").get<std::string>());
    }
    if (j.contains("features")) {
        for (const auto& fj : j.at("features")) {
            layer.features.push_back(json_to_feature(fj));
        }
    }
    return layer;
}

/// Check if any coordinate of a geometry is inside the bounding box.
bool geometry_intersects_bbox(const Geometry& geom, const BoundingBox& bbox) {
    return std::visit(
        [&bbox](const auto& g) -> bool {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, Point>) {
                return bbox.contains(g.position);
            } else if constexpr (std::is_same_v<T, LineString>) {
                for (const auto& v : g.vertices) {
                    if (bbox.contains(v)) return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, Polygon>) {
                for (const auto& ring : g.rings) {
                    for (const auto& v : ring) {
                        if (bbox.contains(v)) return true;
                    }
                }
                return false;
            }
            return false;
        },
        geom);
}

}  // namespace

// --- FilePersistenceAdapter implementation ---

FilePersistenceAdapter::FilePersistenceAdapter(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
    ensure_directory_exists();
}

std::filesystem::path FilePersistenceAdapter::layer_path(
    const std::string& name) const {
    return data_dir_ / (name + ".json");
}

void FilePersistenceAdapter::ensure_directory_exists() {
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);
    // Silently ignore errors here — write operations will report failures.
}

std::expected<void, PersistenceError>
FilePersistenceAdapter::save_layer(const core::domain::Layer& layer) {
    auto path = layer_path(layer.name);

    // Check for duplicate
    if (std::filesystem::exists(path)) {
        return std::unexpected(PersistenceError::DuplicateLayer);
    }

    json j = layer_to_json(layer);

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return std::unexpected(PersistenceError::WriteError);
    }
    ofs << j.dump(2);
    if (!ofs.good()) {
        return std::unexpected(PersistenceError::WriteError);
    }
    return {};
}

std::expected<Layer, PersistenceError>
FilePersistenceAdapter::find_layer(const std::string& name) {
    auto path = layer_path(name);

    if (!std::filesystem::exists(path)) {
        return std::unexpected(PersistenceError::NotFound);
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return std::unexpected(PersistenceError::ConnectionError);
    }

    try {
        json j = json::parse(ifs);
        return json_to_layer(j);
    } catch (const json::exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    }
}

std::expected<std::vector<std::string>, PersistenceError>
FilePersistenceAdapter::list_layers() {
    std::vector<std::string> names;

    std::error_code ec;
    if (!std::filesystem::exists(data_dir_, ec)) {
        return names;  // Empty list if directory doesn't exist
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(data_dir_, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            names.push_back(entry.path().stem().string());
        }
    }

    return names;
}

std::expected<void, PersistenceError>
FilePersistenceAdapter::remove_layer(const std::string& name) {
    auto path = layer_path(name);

    if (!std::filesystem::exists(path)) {
        return std::unexpected(PersistenceError::NotFound);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        return std::unexpected(PersistenceError::WriteError);
    }
    return {};
}

std::expected<std::vector<GeoFeature>, PersistenceError>
FilePersistenceAdapter::query_features(const BoundingBox& extent) {
    std::vector<GeoFeature> result;

    std::error_code ec;
    if (!std::filesystem::exists(data_dir_, ec)) {
        return result;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(data_dir_, ec)) {
        if (ec) break;
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        std::ifstream ifs(entry.path());
        if (!ifs.is_open()) continue;

        try {
            json j = json::parse(ifs);
            if (!j.contains("features")) continue;

            for (const auto& fj : j.at("features")) {
                auto feature = json_to_feature(fj);
                if (geometry_intersects_bbox(feature.geometry, extent)) {
                    result.push_back(std::move(feature));
                }
            }
        } catch (const json::exception&) {
            // Skip malformed files during query
            continue;
        }
    }

    return result;
}

}  // namespace garraiobide::adapters::persistence
