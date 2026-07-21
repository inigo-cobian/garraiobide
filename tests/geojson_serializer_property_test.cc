#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "src/adapters/http/geojson_serializer.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"
#include "src/core/domain/properties.h"

namespace garraiobide::adapters::http {
namespace {

using nlohmann::json;
using namespace garraiobide::core::domain;

// ---------------------------------------------------------------------------
// Test infrastructure: deterministic pseudo-random generators for domain objects
// ---------------------------------------------------------------------------

class GeoJsonPropertyTest : public ::testing::Test {
   protected:
    std::mt19937 rng_{42};  // Fixed seed for reproducibility

    double random_latitude() {
        std::uniform_real_distribution<double> dist(-90.0, 90.0);
        return dist(rng_);
    }

    double random_longitude() {
        std::uniform_real_distribution<double> dist(-180.0, 180.0);
        return dist(rng_);
    }

    Coordinate random_coordinate() {
        return Coordinate{random_latitude(), random_longitude()};
    }

    Point random_point() { return Point{random_coordinate()}; }

    LineString random_linestring() {
        std::uniform_int_distribution<int> size_dist(2, 10);
        int n = size_dist(rng_);
        std::vector<Coordinate> vertices;
        vertices.reserve(n);
        for (int i = 0; i < n; ++i) {
            vertices.push_back(random_coordinate());
        }
        return LineString{std::move(vertices)};
    }

    Polygon random_polygon() {
        // Generate a single outer ring with 4-8 vertices (closed ring).
        std::uniform_int_distribution<int> size_dist(4, 8);
        int n = size_dist(rng_);
        std::vector<Coordinate> ring;
        ring.reserve(n + 1);
        for (int i = 0; i < n; ++i) {
            ring.push_back(random_coordinate());
        }
        ring.push_back(ring.front());  // Close the ring
        return Polygon{{std::move(ring)}};
    }

    Geometry random_geometry() {
        std::uniform_int_distribution<int> dist(0, 2);
        switch (dist(rng_)) {
            case 0:
                return random_point();
            case 1:
                return random_linestring();
            default:
                return random_polygon();
        }
    }

    Properties random_properties() {
        Properties props;
        std::uniform_int_distribution<int> count_dist(0, 5);
        int count = count_dist(rng_);

        for (int i = 0; i < count; ++i) {
            std::string key = "prop_" + std::to_string(i);
            std::uniform_int_distribution<int> type_dist(0, 3);
            switch (type_dist(rng_)) {
                case 0:
                    props[key] = std::string("value_" + std::to_string(i));
                    break;
                case 1: {
                    std::uniform_real_distribution<double> d(-1000.0, 1000.0);
                    props[key] = d(rng_);
                    break;
                }
                case 2: {
                    std::uniform_int_distribution<int64_t> d(-1000, 1000);
                    props[key] = d(rng_);
                    break;
                }
                case 3:
                    props[key] = (rng_() % 2 == 0);
                    break;
            }
        }
        return props;
    }

    GeoFeature random_feature_with_geometry(const Geometry& geom) {
        GeoFeature f;
        if (rng_() % 2 == 0) {
            f.id = "feat_" + std::to_string(rng_() % 10000);
        }
        f.geometry = geom;
        f.properties = random_properties();
        return f;
    }

    GeoFeature random_feature() {
        return random_feature_with_geometry(random_geometry());
    }

    Layer random_layer(int feature_count) {
        Layer layer;
        layer.name = "test_layer_" + std::to_string(rng_() % 1000);
        layer.scale = (rng_() % 2 == 0) ? SpatialScale::Urban
                                         : SpatialScale::Regional;
        for (int i = 0; i < feature_count; ++i) {
            layer.features.push_back(random_feature());
        }
        return layer;
    }
};

// ---------------------------------------------------------------------------
// Property 1: GeoJSON geometry serialization preserves structure with
//             coordinate swap
// Validates: Requirements 2.1, 2.2, 2.3, 2.7
// ---------------------------------------------------------------------------

TEST_F(GeoJsonPropertyTest,
       PointSerializationProducesCorrectTypeAndSwappedCoordinates) {
    // **Validates: Requirements 2.1, 2.7**
    constexpr int kTrials = 100;

    for (int i = 0; i < kTrials; ++i) {
        Point pt = random_point();
        GeoFeature feature = random_feature_with_geometry(pt);

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        // (a) Geometry type matches variant type
        ASSERT_EQ(j["geometry"]["type"], "Point")
            << "Trial " << i << ": Expected geometry type 'Point'";

        // (b) Coordinate swap: domain (lat, lng) → GeoJSON [lng, lat]
        auto coords = j["geometry"]["coordinates"];
        ASSERT_EQ(coords.size(), 2u);
        EXPECT_DOUBLE_EQ(coords[0].get<double>(), pt.position.longitude)
            << "Trial " << i << ": GeoJSON[0] should be longitude";
        EXPECT_DOUBLE_EQ(coords[1].get<double>(), pt.position.latitude)
            << "Trial " << i << ": GeoJSON[1] should be latitude";
    }
}

TEST_F(GeoJsonPropertyTest,
       LineStringSerializationProducesCorrectTypeAndSwappedCoordinates) {
    // **Validates: Requirements 2.2, 2.7**
    constexpr int kTrials = 100;

    for (int i = 0; i < kTrials; ++i) {
        LineString ls = random_linestring();
        GeoFeature feature = random_feature_with_geometry(ls);

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        // (a) Geometry type matches
        ASSERT_EQ(j["geometry"]["type"], "LineString")
            << "Trial " << i << ": Expected geometry type 'LineString'";

        // (b) Every coordinate pair is [longitude, latitude]
        auto coords = j["geometry"]["coordinates"];
        ASSERT_EQ(coords.size(), ls.vertices.size());
        for (size_t v = 0; v < ls.vertices.size(); ++v) {
            EXPECT_DOUBLE_EQ(coords[v][0].get<double>(),
                             ls.vertices[v].longitude)
                << "Trial " << i << ", vertex " << v;
            EXPECT_DOUBLE_EQ(coords[v][1].get<double>(),
                             ls.vertices[v].latitude)
                << "Trial " << i << ", vertex " << v;
        }
    }
}

TEST_F(GeoJsonPropertyTest,
       PolygonSerializationProducesCorrectTypeAndSwappedCoordinates) {
    // **Validates: Requirements 2.3, 2.7**
    constexpr int kTrials = 100;

    for (int i = 0; i < kTrials; ++i) {
        Polygon poly = random_polygon();
        GeoFeature feature = random_feature_with_geometry(poly);

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        // (a) Geometry type matches
        ASSERT_EQ(j["geometry"]["type"], "Polygon")
            << "Trial " << i << ": Expected geometry type 'Polygon'";

        // (b) Coordinate swap for every vertex in every ring
        auto rings = j["geometry"]["coordinates"];
        ASSERT_EQ(rings.size(), poly.rings.size());
        for (size_t r = 0; r < poly.rings.size(); ++r) {
            ASSERT_EQ(rings[r].size(), poly.rings[r].size());
            for (size_t v = 0; v < poly.rings[r].size(); ++v) {
                EXPECT_DOUBLE_EQ(rings[r][v][0].get<double>(),
                                 poly.rings[r][v].longitude)
                    << "Trial " << i << ", ring " << r << ", vertex " << v;
                EXPECT_DOUBLE_EQ(rings[r][v][1].get<double>(),
                                 poly.rings[r][v].latitude)
                    << "Trial " << i << ", ring " << r << ", vertex " << v;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Property 2: GeoJSON properties and id preservation
// Validates: Requirements 2.4, 2.5
// ---------------------------------------------------------------------------

TEST_F(GeoJsonPropertyTest, PropertiesPreservedWithCorrectJsonTypes) {
    // **Validates: Requirements 2.4**
    constexpr int kTrials = 100;

    for (int i = 0; i < kTrials; ++i) {
        GeoFeature feature = random_feature();

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        auto props_json = j["properties"];

        // Every domain property key is present with correct type
        for (const auto& [key, value] : feature.properties) {
            ASSERT_TRUE(props_json.contains(key))
                << "Trial " << i << ": Missing key '" << key << "'";

            std::visit(
                [&](const auto& v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        EXPECT_TRUE(props_json[key].is_string())
                            << "Trial " << i << ": Key '" << key
                            << "' should be string";
                        EXPECT_EQ(props_json[key].get<std::string>(), v);
                    } else if constexpr (std::is_same_v<T, double>) {
                        EXPECT_TRUE(props_json[key].is_number())
                            << "Trial " << i << ": Key '" << key
                            << "' should be number";
                        EXPECT_DOUBLE_EQ(props_json[key].get<double>(), v);
                    } else if constexpr (std::is_same_v<T, int64_t>) {
                        EXPECT_TRUE(props_json[key].is_number())
                            << "Trial " << i << ": Key '" << key
                            << "' should be number";
                        EXPECT_EQ(props_json[key].get<int64_t>(), v);
                    } else if constexpr (std::is_same_v<T, bool>) {
                        EXPECT_TRUE(props_json[key].is_boolean())
                            << "Trial " << i << ": Key '" << key
                            << "' should be boolean";
                        EXPECT_EQ(props_json[key].get<bool>(), v);
                    }
                },
                value);
        }

        // No extra keys in the output beyond what was in the domain object
        EXPECT_EQ(props_json.size(), feature.properties.size())
            << "Trial " << i << ": Property count mismatch";
    }
}

TEST_F(GeoJsonPropertyTest, IdPreservedWhenPresent) {
    // **Validates: Requirements 2.5**
    constexpr int kTrials = 100;

    for (int i = 0; i < kTrials; ++i) {
        GeoFeature feature = random_feature();
        // Force id to be set for this property
        feature.id = "id_" + std::to_string(i);

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        ASSERT_TRUE(j.contains("id"))
            << "Trial " << i << ": Missing 'id' field";
        EXPECT_EQ(j["id"].get<std::string>(), feature.id.value())
            << "Trial " << i << ": Id value mismatch";
    }
}

TEST_F(GeoJsonPropertyTest, IdAbsentWhenNotSet) {
    // **Validates: Requirements 2.5**
    constexpr int kTrials = 50;

    for (int i = 0; i < kTrials; ++i) {
        GeoFeature feature = random_feature();
        feature.id = std::nullopt;  // Force no id

        std::string output = GeoJsonSerializer::serialize_feature(feature);
        json j = json::parse(output);

        EXPECT_FALSE(j.contains("id"))
            << "Trial " << i
            << ": 'id' field should not be present when id is nullopt";
    }
}

// ---------------------------------------------------------------------------
// Property 3: GeoJSON FeatureCollection completeness
// Validates: Requirements 2.6
// ---------------------------------------------------------------------------

TEST_F(GeoJsonPropertyTest, FeatureCollectionContainsExactlyNFeatures) {
    // **Validates: Requirements 2.6**
    constexpr int kTrials = 50;

    for (int i = 0; i < kTrials; ++i) {
        std::uniform_int_distribution<int> count_dist(0, 20);
        int n = count_dist(rng_);
        Layer layer = random_layer(n);

        std::string output = GeoJsonSerializer::serialize_layer(layer);
        json j = json::parse(output);

        ASSERT_EQ(j["type"], "FeatureCollection")
            << "Trial " << i << ": Expected type 'FeatureCollection'";
        ASSERT_EQ(j["features"].size(), static_cast<size_t>(n))
            << "Trial " << i << ": Expected " << n << " features, got "
            << j["features"].size();

        // Each element is a Feature
        for (int f = 0; f < n; ++f) {
            EXPECT_EQ(j["features"][f]["type"], "Feature")
                << "Trial " << i << ", feature " << f;
        }
    }
}

TEST_F(GeoJsonPropertyTest,
       FeatureCollectionPreservesOrderAndContent) {
    // **Validates: Requirements 2.6**
    constexpr int kTrials = 30;

    for (int i = 0; i < kTrials; ++i) {
        std::uniform_int_distribution<int> count_dist(1, 10);
        int n = count_dist(rng_);
        Layer layer = random_layer(n);

        std::string output = GeoJsonSerializer::serialize_layer(layer);
        json collection = json::parse(output);

        // Verify order preservation by checking that each feature in the
        // collection matches the corresponding feature serialized individually.
        for (int f = 0; f < n; ++f) {
            std::string individual =
                GeoJsonSerializer::serialize_feature(layer.features[f]);
            json expected = json::parse(individual);

            EXPECT_EQ(collection["features"][f], expected)
                << "Trial " << i << ", feature " << f
                << ": Collection entry differs from individual serialization";
        }
    }
}

}  // namespace
}  // namespace garraiobide::adapters::http
