#include "mongo_persistence_adapter.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/write_concern.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/uri.hpp>

namespace garraiobide::adapters::persistence {

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

using bsoncxx::builder::basic::array;
using bsoncxx::builder::basic::document;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

namespace {

// --- mongocxx::instance singleton ---

std::once_flag instance_flag;
std::unique_ptr<mongocxx::instance> driver_instance;

void ensure_driver_instance() {
    std::call_once(instance_flag, [] {
        driver_instance = std::make_unique<mongocxx::instance>();
    });
}

// --- Scale helpers ---

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

// --- Domain → BSON serialization helpers ---

bsoncxx::document::value geometry_to_bson(const Geometry& geom) {
    return std::visit(
        [](const auto& g) -> bsoncxx::document::value {
            using T = std::decay_t<decltype(g)>;
            if constexpr (std::is_same_v<T, Point>) {
                return make_document(
                    kvp("type", "Point"),
                    kvp("coordinates", make_array(g.position.longitude, g.position.latitude)));
            } else if constexpr (std::is_same_v<T, LineString>) {
                array coords;
                for (const auto& v : g.vertices) {
                    coords.append(make_array(v.longitude, v.latitude));
                }
                return make_document(
                    kvp("type", "LineString"),
                    kvp("coordinates", coords.extract()));
            } else if constexpr (std::is_same_v<T, Polygon>) {
                array rings_arr;
                for (const auto& ring : g.rings) {
                    array ring_arr;
                    for (const auto& v : ring) {
                        ring_arr.append(make_array(v.longitude, v.latitude));
                    }
                    rings_arr.append(ring_arr.extract());
                }
                return make_document(
                    kvp("type", "Polygon"),
                    kvp("coordinates", rings_arr.extract()));
            }
        },
        geom);
}

bsoncxx::document::value properties_to_bson(const Properties& props) {
    document doc;
    for (const auto& [key, val] : props) {
        std::visit(
            [&doc, &key](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    doc.append(kvp(key, v));
                } else if constexpr (std::is_same_v<T, double>) {
                    doc.append(kvp(key, v));
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    doc.append(kvp(key, v));
                } else if constexpr (std::is_same_v<T, bool>) {
                    doc.append(kvp(key, v));
                }
            },
            val);
    }
    return doc.extract();
}

bsoncxx::document::value feature_to_bson(const GeoFeature& feature) {
    document doc;
    if (feature.id.has_value()) {
        doc.append(kvp("id", feature.id.value()));
    }
    doc.append(kvp("geometry", geometry_to_bson(feature.geometry)));
    doc.append(kvp("properties", properties_to_bson(feature.properties)));
    return doc.extract();
}

bsoncxx::document::value layer_to_bson(const Layer& layer) {
    array features_arr;
    for (const auto& f : layer.features) {
        features_arr.append(feature_to_bson(f));
    }

    return make_document(
        kvp("name", layer.name),
        kvp("scale", scale_to_string(layer.scale)),
        kvp("features", features_arr.extract()));
}

// --- BSON → Domain deserialization helpers ---

Coordinate bson_to_coordinate(bsoncxx::array::view coords) {
    auto it = coords.begin();
    double longitude = it->get_double().value;
    ++it;
    double latitude = it->get_double().value;
    return Coordinate{.latitude = latitude, .longitude = longitude};
}

Geometry bson_to_geometry(bsoncxx::document::view doc) {
    auto type = std::string{doc["type"].get_string().value};
    auto coordinates = doc["coordinates"];

    if (type == "Point") {
        return Point{.position = bson_to_coordinate(coordinates.get_array().value)};
    }

    if (type == "LineString") {
        std::vector<Coordinate> vertices;
        for (const auto& coord : coordinates.get_array().value) {
            vertices.push_back(bson_to_coordinate(coord.get_array().value));
        }
        return LineString{.vertices = std::move(vertices)};
    }

    // Polygon
    std::vector<std::vector<Coordinate>> rings;
    for (const auto& ring : coordinates.get_array().value) {
        std::vector<Coordinate> ring_coords;
        for (const auto& coord : ring.get_array().value) {
            ring_coords.push_back(bson_to_coordinate(coord.get_array().value));
        }
        rings.push_back(std::move(ring_coords));
    }
    return Polygon{.rings = std::move(rings)};
}

PropertyValue bson_to_property_value(bsoncxx::document::element elem) {
    switch (elem.type()) {
        case bsoncxx::type::k_utf8:
            return std::string{elem.get_string().value};
        case bsoncxx::type::k_double:
            return elem.get_double().value;
        case bsoncxx::type::k_int64:
            return elem.get_int64().value;
        case bsoncxx::type::k_int32:
            return static_cast<int64_t>(elem.get_int32().value);
        case bsoncxx::type::k_bool:
            return elem.get_bool().value;
        default:
            // Fallback: treat as string via JSON representation
            return std::string{"<unsupported>"};
    }
}

Properties bson_to_properties(bsoncxx::document::view doc) {
    Properties props;
    for (const auto& elem : doc) {
        props[std::string{elem.key()}] = bson_to_property_value(elem);
    }
    return props;
}

GeoFeature bson_to_feature(bsoncxx::document::view doc) {
    GeoFeature feature;

    auto id_it = doc.find("id");
    if (id_it != doc.end() && id_it->type() == bsoncxx::type::k_utf8) {
        feature.id = std::string{id_it->get_string().value};
    }

    feature.geometry = bson_to_geometry(doc["geometry"].get_document().value);

    auto props_it = doc.find("properties");
    if (props_it != doc.end() && props_it->type() == bsoncxx::type::k_document) {
        feature.properties = bson_to_properties(props_it->get_document().value);
    }

    return feature;
}

Layer bson_to_layer(bsoncxx::document::view doc) {
    Layer layer;
    layer.name = std::string{doc["name"].get_string().value};

    auto scale_it = doc.find("scale");
    if (scale_it != doc.end() && scale_it->type() == bsoncxx::type::k_utf8) {
        layer.scale = string_to_scale(std::string{scale_it->get_string().value});
    }

    auto features_it = doc.find("features");
    if (features_it != doc.end() && features_it->type() == bsoncxx::type::k_array) {
        for (const auto& f : features_it->get_array().value) {
            layer.features.push_back(bson_to_feature(f.get_document().value));
        }
    }

    return layer;
}

}  // namespace

// --- MongoPersistenceAdapter implementation ---

MongoPersistenceAdapter::MongoPersistenceAdapter(std::string connection_string,
                                                 std::string database_name)
    : connection_string_(std::move(connection_string)),
      database_name_(std::move(database_name)) {
    if (connection_string_.empty()) {
        throw std::invalid_argument("connection_string must not be empty");
    }
    if (database_name_.empty()) {
        throw std::invalid_argument("database_name must not be empty");
    }

    ensure_driver_instance();

    mongocxx::uri uri{connection_string_};
    client_ = mongocxx::client{uri};
    db_ = client_[database_name_];
    layers_collection_ = db_["layers"];

    // Retry index creation to handle CI timing where MongoDB may not be fully ready.
    constexpr int kMaxIndexRetries = 3;
    constexpr auto kIndexRetryDelay = std::chrono::milliseconds(200);
    bool indexes_created = false;
    for (int attempt = 0; attempt < kMaxIndexRetries; ++attempt) {
        auto idx_result = ensure_indexes();
        if (idx_result.has_value()) {
            indexes_created = true;
            break;
        }
        if (attempt < kMaxIndexRetries - 1) {
            std::this_thread::sleep_for(kIndexRetryDelay);
        }
    }
    if (!indexes_created) {
        throw std::runtime_error("Failed to create required indexes after retries");
    }
}

std::expected<void, PersistenceError> MongoPersistenceAdapter::ensure_indexes() {
    try {
        // 1. Unique index on "name"
        auto name_index = make_document(kvp("name", 1));

        mongocxx::options::index name_opts;
        name_opts.unique(true);
        layers_collection_.create_index(name_index.view(), name_opts);

        // 2. 2dsphere index on "features.geometry"
        auto geo_index = make_document(kvp("features.geometry", "2dsphere"));
        layers_collection_.create_index(geo_index.view());

        return {};
    } catch (const mongocxx::exception& e) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (...) {
        return std::unexpected(PersistenceError::ConnectionError);
    }
}

// --- PersistencePort method implementations ---

std::expected<void, PersistenceError>
MongoPersistenceAdapter::save_layer(const core::domain::Layer& layer) {
    // Validate non-empty layer name
    if (layer.name.empty()) {
        return std::unexpected(PersistenceError::WriteError);
    }

    try {
        auto doc = layer_to_bson(layer);

        // Add explicit write concern to ensure acknowledged write
        mongocxx::options::insert insert_opts;
        mongocxx::write_concern wc;
        wc.acknowledge_level(mongocxx::write_concern::level::k_majority);
        insert_opts.write_concern(wc);

        auto result = layers_collection_.insert_one(doc.view(), insert_opts);

        // Validate insert result
        if (!result) {
            return std::unexpected(PersistenceError::WriteError);
        }
        if (result->inserted_id().type() == bsoncxx::type::k_null) {
            return std::unexpected(PersistenceError::WriteError);
        }

        return {};
    } catch (const mongocxx::bulk_write_exception& e) {
        // Error code 11000 = duplicate key violation
        if (e.code().value() == 11000) {
            return std::unexpected(PersistenceError::DuplicateLayer);
        }
        return std::unexpected(PersistenceError::WriteError);
    } catch (const mongocxx::exception&) {
        return std::unexpected(PersistenceError::WriteError);
    } catch (...) {
        return std::unexpected(PersistenceError::WriteError);
    }
}

std::expected<Layer, PersistenceError>
MongoPersistenceAdapter::find_layer(const std::string& name) {
    try {
        auto filter = make_document(kvp("name", name));
        auto result = layers_collection_.find_one(filter.view());

        if (!result) {
            return std::unexpected(PersistenceError::NotFound);
        }

        return bson_to_layer(result->view());
    } catch (const mongocxx::operation_exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::logic_error&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (...) {
        return std::unexpected(PersistenceError::ConnectionError);
    }
}

std::expected<std::vector<std::string>, PersistenceError>
MongoPersistenceAdapter::list_layers() {
    try {
        auto filter = make_document();
        auto projection = make_document(kvp("name", 1), kvp("_id", 0));

        mongocxx::options::find opts;
        opts.projection(projection.view());

        auto cursor = layers_collection_.find(filter.view(), opts);

        std::vector<std::string> names;
        for (const auto& doc : cursor) {
            auto name_it = doc.find("name");
            if (name_it != doc.end() && name_it->type() == bsoncxx::type::k_utf8) {
                names.emplace_back(std::string{name_it->get_string().value});
            }
        }
        return names;
    } catch (const mongocxx::operation_exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::logic_error&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (...) {
        return std::unexpected(PersistenceError::ConnectionError);
    }
}

std::expected<void, PersistenceError>
MongoPersistenceAdapter::remove_layer(const std::string& name) {
    try {
        auto filter = make_document(kvp("name", name));
        auto result = layers_collection_.delete_one(filter.view());

        if (!result || result->deleted_count() == 0) {
            return std::unexpected(PersistenceError::NotFound);
        }

        return {};
    } catch (const mongocxx::operation_exception&) {
        return std::unexpected(PersistenceError::WriteError);
    } catch (const mongocxx::logic_error&) {
        return std::unexpected(PersistenceError::WriteError);
    } catch (const mongocxx::exception&) {
        return std::unexpected(PersistenceError::WriteError);
    } catch (...) {
        return std::unexpected(PersistenceError::WriteError);
    }
}

std::expected<std::vector<GeoFeature>, PersistenceError>
MongoPersistenceAdapter::query_features(const BoundingBox& extent) {
    // Validate bounding box: inverted → return empty immediately
    if (extent.south_west.latitude > extent.north_east.latitude ||
        extent.south_west.longitude > extent.north_east.longitude) {
        return std::vector<GeoFeature>{};
    }

    try {
        // Construct GeoJSON polygon from BoundingBox corners
        // Closed ring: SW → NW → NE → SE → SW in [lon, lat] order
        double sw_lon = extent.south_west.longitude;
        double sw_lat = extent.south_west.latitude;
        double ne_lon = extent.north_east.longitude;
        double ne_lat = extent.north_east.latitude;

        auto bbox_polygon = make_document(
            kvp("type", "Polygon"),
            kvp("coordinates", make_array(
                make_array(
                    make_array(sw_lon, sw_lat),   // SW
                    make_array(sw_lon, ne_lat),   // NW
                    make_array(ne_lon, ne_lat),   // NE
                    make_array(ne_lon, sw_lat),   // SE
                    make_array(sw_lon, sw_lat)    // SW (close ring)
                )
            ))
        );

        auto match_filter = make_document(
            kvp("features.geometry", make_document(
                kvp("$geoIntersects", make_document(
                    kvp("$geometry", bbox_polygon.view())
                ))
            ))
        );

        auto replace_root_expr = make_document(kvp("newRoot", "$features"));

        mongocxx::pipeline pipeline;
        pipeline.unwind("$features");
        pipeline.match(match_filter.view());
        pipeline.replace_root(replace_root_expr.view());

        auto cursor = layers_collection_.aggregate(pipeline);

        std::vector<GeoFeature> features;
        for (const auto& doc : cursor) {
            features.push_back(bson_to_feature(doc));
        }
        return features;
    } catch (const mongocxx::operation_exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::logic_error&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (const mongocxx::exception&) {
        return std::unexpected(PersistenceError::ConnectionError);
    } catch (...) {
        return std::unexpected(PersistenceError::ConnectionError);
    }
}

}  // namespace garraiobide::adapters::persistence
