#include <gtest/gtest.h>

#include <string>
#include <chrono>
#include <thread>
#include <vector>

#include "../src/adapters/persistence/mongo_persistence_adapter.h"
#include "../src/core/domain/bounding_box.h"
#include "../src/core/domain/coordinate.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"
#include "../src/core/domain/layer.h"
#include "../src/core/domain/properties.h"
#include "../src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {
namespace {

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

// =============================================================================
// Comparison helpers
// =============================================================================

bool coordinates_equal(const Coordinate& a, const Coordinate& b) {
    return a.latitude == b.latitude && a.longitude == b.longitude;
}

bool geometry_equal(const Geometry& a, const Geometry& b) {
    if (a.index() != b.index()) return false;

    return std::visit(
        [&b](const auto& ga) -> bool {
            using T = std::decay_t<decltype(ga)>;
            const auto& gb = std::get<T>(b);

            if constexpr (std::is_same_v<T, Point>) {
                return coordinates_equal(ga.position, gb.position);
            } else if constexpr (std::is_same_v<T, LineString>) {
                if (ga.vertices.size() != gb.vertices.size()) return false;
                for (size_t i = 0; i < ga.vertices.size(); ++i) {
                    if (!coordinates_equal(ga.vertices[i], gb.vertices[i]))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, Polygon>) {
                if (ga.rings.size() != gb.rings.size()) return false;
                for (size_t r = 0; r < ga.rings.size(); ++r) {
                    if (ga.rings[r].size() != gb.rings[r].size()) return false;
                    for (size_t i = 0; i < ga.rings[r].size(); ++i) {
                        if (!coordinates_equal(ga.rings[r][i], gb.rings[r][i]))
                            return false;
                    }
                }
                return true;
            }
            return false;
        },
        a);
}

bool properties_equal(const Properties& a, const Properties& b) {
    if (a.size() != b.size()) return false;
    for (const auto& [key, val] : a) {
        auto it = b.find(key);
        if (it == b.end()) return false;
        if (val != it->second) return false;
    }
    return true;
}

bool feature_equal(const GeoFeature& a, const GeoFeature& b) {
    if (a.id != b.id) return false;
    if (!geometry_equal(a.geometry, b.geometry)) return false;
    if (!properties_equal(a.properties, b.properties)) return false;
    return true;
}

bool layer_equal(const Layer& a, const Layer& b) {
    if (a.name != b.name) return false;
    if (a.scale != b.scale) return false;
    if (a.features.size() != b.features.size()) return false;
    for (size_t i = 0; i < a.features.size(); ++i) {
        if (!feature_equal(a.features[i], b.features[i])) return false;
    }
    return true;
}

// =============================================================================
// Test fixture
// =============================================================================

class MongoIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        connection_string_ = "mongodb://localhost:27017";
        database_name_ = "integration_test_" + std::to_string(test_counter_++);
        adapter_ = std::make_unique<MongoPersistenceAdapter>(
            connection_string_, database_name_);

        // Verify MongoDB connection is operational before running tests.
        constexpr int kMaxRetries = 10;
        constexpr auto kRetryDelay = std::chrono::milliseconds(500);
        for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
            auto result = adapter_->list_layers();
            if (result.has_value()) break;
            if (attempt == kMaxRetries - 1) {
                FAIL() << "MongoDB not operational after " << kMaxRetries << " retries";
            }
            std::this_thread::sleep_for(kRetryDelay);
        }
    }

    void TearDown() override {
        adapter_.reset();
        // Drop the test database to clean up
        try {
            mongocxx::client client{mongocxx::uri{connection_string_}};
            client[database_name_].drop();
        } catch (...) {
            // Best effort cleanup
        }
    }

    std::string connection_string_;
    std::string database_name_;
    std::unique_ptr<MongoPersistenceAdapter> adapter_;

   private:
    static inline int test_counter_ = 0;
};

// =============================================================================
// Test: valid connection succeeds and adapter is operational
// Validates: Requirement 1.1
// =============================================================================

TEST_F(MongoIntegrationTest, ValidConnectionSucceeds) {
    Layer layer{
        .name = "operational_test",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "op1",
            .geometry = Point{{43.26, -2.93}},
            .properties = {{"status", std::string("ok")}},
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value()) << "Adapter should be operational after valid connection";

    // Retry find_layer — under CI load, read-after-write may need time.
    std::expected<Layer, PersistenceError> find_result;
    for (int attempt = 0; attempt < 10; ++attempt) {
        find_result = adapter_->find_layer("operational_test");
        if (find_result.has_value()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    ASSERT_TRUE(find_result.has_value())
        << "Adapter should retrieve saved layers (error: "
        << (find_result.error() == PersistenceError::NotFound ? "NotFound" : "ConnectionError")
        << ")";
    EXPECT_EQ(find_result.value().name, "operational_test");
}

// =============================================================================
// Test: empty connection string -> ConnectionError (throws at construction)
// Validates: Requirement 1.5
// =============================================================================

TEST_F(MongoIntegrationTest, EmptyConnectionStringThrows) {
    EXPECT_THROW(
        MongoPersistenceAdapter("", "some_database"),
        std::invalid_argument);
}

// =============================================================================
// Test: empty database name -> ConnectionError (throws at construction)
// Validates: Requirement 1.5
// =============================================================================

TEST_F(MongoIntegrationTest, EmptyDatabaseNameThrows) {
    EXPECT_THROW(
        MongoPersistenceAdapter("mongodb://localhost:27017", ""),
        std::invalid_argument);
}

// =============================================================================
// Test: index idempotency (construct adapter twice, no errors)
// Validates: Requirement 8.3
// =============================================================================

TEST_F(MongoIntegrationTest, IndexIdempotency) {
    // First adapter was already constructed in SetUp with indexes.
    // Constructing a second adapter pointing to the same database should succeed.
    EXPECT_NO_THROW({
        MongoPersistenceAdapter second(connection_string_, database_name_);
    });

    // Both adapters should be operational — verify using the first one.
    Layer layer{
        .name = "idempotency_test",
        .scale = SpatialScale::Regional,
        .features = {},
    };
    auto result = adapter_->save_layer(layer);
    ASSERT_TRUE(result.has_value())
        << "Adapter should remain operational after second adapter creates indexes";
}

// =============================================================================
// Test: empty layer name -> WriteError
// Validates: Requirement 2.7
// =============================================================================

TEST_F(MongoIntegrationTest, EmptyLayerNameReturnsWriteError) {
    Layer layer{
        .name = "",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "f1",
            .geometry = Point{{43.26, -2.93}},
            .properties = {},
        }},
    };

    auto result = adapter_->save_layer(layer);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PersistenceError::WriteError);
}

// =============================================================================
// Test: empty features vector round-trip
// Validates: Requirement 2.6
// =============================================================================

TEST_F(MongoIntegrationTest, EmptyFeaturesVectorRoundTrip) {
    Layer layer{
        .name = "empty_features_layer",
        .scale = SpatialScale::Urban,
        .features = {},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value()) << "Saving layer with empty features should succeed";

    auto find_result = adapter_->find_layer("empty_features_layer");
    ASSERT_TRUE(find_result.has_value()) << "Finding layer with empty features should succeed";

    const auto& loaded = find_result.value();
    EXPECT_EQ(loaded.name, "empty_features_layer");
    EXPECT_EQ(loaded.scale, SpatialScale::Urban);
    EXPECT_TRUE(loaded.features.empty());
}

// =============================================================================
// Test: inverted bounding box -> empty result
// Validates: Requirement 6.6
// =============================================================================

TEST_F(MongoIntegrationTest, InvertedBoundingBoxReturnsEmpty) {
    // Save a layer with features to ensure there's data
    Layer layer{
        .name = "bbox_test_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "everywhere",
            .geometry = Point{{0.0, 0.0}},
            .properties = {},
        }},
    };
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    // Inverted latitude: south > north
    BoundingBox inverted_lat{
        .south_west = Coordinate{50.0, -10.0},
        .north_east = Coordinate{10.0, 10.0},
    };
    auto result1 = adapter_->query_features(inverted_lat);
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value().empty()) << "Inverted latitude bbox should return empty";

    // Inverted longitude: west > east
    BoundingBox inverted_lon{
        .south_west = Coordinate{-10.0, 50.0},
        .north_east = Coordinate{10.0, -50.0},
    };
    auto result2 = adapter_->query_features(inverted_lon);
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2.value().empty()) << "Inverted longitude bbox should return empty";
}

// =============================================================================
// Test: all three geometry types individually round-trip
// Validates: Requirements 7.1, 7.2, 7.3
// =============================================================================

TEST_F(MongoIntegrationTest, PointGeometryRoundTrip) {
    Layer layer{
        .name = "point_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "pt1",
            .geometry = Point{{43.2630, -2.9349}},
            .properties = {{"type", std::string("stop")}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("point_rt");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(layer_equal(layer, result.value()))
        << "Point geometry should round-trip exactly";
}

TEST_F(MongoIntegrationTest, LineStringGeometryRoundTrip) {
    Layer layer{
        .name = "linestring_rt",
        .scale = SpatialScale::Regional,
        .features = {GeoFeature{
            .id = "ls1",
            .geometry = LineString{{
                {43.25, -2.95},
                {43.26, -2.93},
                {43.27, -2.91},
                {43.28, -2.89},
            }},
            .properties = {{"route_id", std::string("A1")}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("linestring_rt");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(layer_equal(layer, result.value()))
        << "LineString geometry should round-trip exactly";
}

TEST_F(MongoIntegrationTest, PolygonGeometryRoundTrip) {
    Layer layer{
        .name = "polygon_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "poly1",
            .geometry = Polygon{{
                {{43.25, -2.95}, {43.25, -2.90}, {43.28, -2.90},
                 {43.28, -2.95}, {43.25, -2.95}},
            }},
            .properties = {{"zone", std::string("residential")}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("polygon_rt");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(layer_equal(layer, result.value()))
        << "Polygon geometry should round-trip exactly";
}

// =============================================================================
// Test: all four PropertyValue types individually round-trip
// Validates: Requirements 2.4, 3.6
// =============================================================================

TEST_F(MongoIntegrationTest, StringPropertyRoundTrip) {
    Layer layer{
        .name = "string_prop_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "f1",
            .geometry = Point{{0.0, 0.0}},
            .properties = {{"name", std::string("Plaza Moyua")}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("string_prop_rt");
    ASSERT_TRUE(result.has_value());

    const auto& props = result.value().features[0].properties;
    ASSERT_EQ(props.size(), 1u);
    auto it = props.find("name");
    ASSERT_NE(it, props.end());
    ASSERT_TRUE(std::holds_alternative<std::string>(it->second));
    EXPECT_EQ(std::get<std::string>(it->second), "Plaza Moyua");
}

TEST_F(MongoIntegrationTest, DoublePropertyRoundTrip) {
    Layer layer{
        .name = "double_prop_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "f1",
            .geometry = Point{{0.0, 0.0}},
            .properties = {{"rating", 4.75}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("double_prop_rt");
    ASSERT_TRUE(result.has_value());

    const auto& props = result.value().features[0].properties;
    auto it = props.find("rating");
    ASSERT_NE(it, props.end());
    ASSERT_TRUE(std::holds_alternative<double>(it->second));
    EXPECT_EQ(std::get<double>(it->second), 4.75);
}

TEST_F(MongoIntegrationTest, Int64PropertyRoundTrip) {
    Layer layer{
        .name = "int64_prop_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "f1",
            .geometry = Point{{0.0, 0.0}},
            .properties = {{"passengers", int64_t{1500}}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("int64_prop_rt");
    ASSERT_TRUE(result.has_value());

    const auto& props = result.value().features[0].properties;
    auto it = props.find("passengers");
    ASSERT_NE(it, props.end());
    ASSERT_TRUE(std::holds_alternative<int64_t>(it->second));
    EXPECT_EQ(std::get<int64_t>(it->second), int64_t{1500});
}

TEST_F(MongoIntegrationTest, BoolPropertyRoundTrip) {
    Layer layer{
        .name = "bool_prop_rt",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "f1",
            .geometry = Point{{0.0, 0.0}},
            .properties = {{"accessible", true}, {"deprecated", false}},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto result = adapter_->find_layer("bool_prop_rt");
    ASSERT_TRUE(result.has_value());

    const auto& props = result.value().features[0].properties;
    auto it_true = props.find("accessible");
    ASSERT_NE(it_true, props.end());
    ASSERT_TRUE(std::holds_alternative<bool>(it_true->second));
    EXPECT_TRUE(std::get<bool>(it_true->second));

    auto it_false = props.find("deprecated");
    ASSERT_NE(it_false, props.end());
    ASSERT_TRUE(std::holds_alternative<bool>(it_false->second));
    EXPECT_FALSE(std::get<bool>(it_false->second));
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
