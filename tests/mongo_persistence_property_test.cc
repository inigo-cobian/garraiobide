#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
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
// Random generators
// =============================================================================

namespace {

std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

// --- Primitive generators ---

std::string random_alphanumeric(int min_len = 1, int max_len = 20) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    std::uniform_int_distribution<int> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> char_dist(0, sizeof(chars) - 2);

    int length = len_dist(rng());
    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result.push_back(chars[char_dist(rng())]);
    }
    return result;
}

double random_double(double min_val = -1e6, double max_val = 1e6) {
    std::uniform_real_distribution<double> dist(min_val, max_val);
    return dist(rng());
}

int64_t random_int64(int64_t min_val = -1000000, int64_t max_val = 1000000) {
    std::uniform_int_distribution<int64_t> dist(min_val, max_val);
    return dist(rng());
}

bool random_bool() {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng()) == 1;
}

// --- Domain generators ---

Coordinate random_coordinate() {
    std::uniform_real_distribution<double> lat_dist(-90.0, 90.0);
    std::uniform_real_distribution<double> lon_dist(-180.0, 180.0);
    return Coordinate{lat_dist(rng()), lon_dist(rng())};
}

Point random_point() {
    return Point{random_coordinate()};
}

LineString random_linestring() {
    std::uniform_int_distribution<int> count_dist(2, 10);
    int count = count_dist(rng());

    std::vector<Coordinate> vertices;
    vertices.reserve(count);
    for (int i = 0; i < count; ++i) {
        vertices.push_back(random_coordinate());
    }
    return LineString{std::move(vertices)};
}

Polygon random_polygon() {
    // Generate a small convex polygon around a random center point.
    // This ensures MongoDB accepts the polygon (valid WGS84, less than hemisphere).
    std::uniform_real_distribution<double> center_lat_dist(-85.0, 85.0);
    std::uniform_real_distribution<double> center_lon_dist(-175.0, 175.0);
    std::uniform_real_distribution<double> offset_dist(0.01, 0.1);
    std::uniform_int_distribution<int> vertex_count_dist(3, 8);

    double center_lat = center_lat_dist(rng());
    double center_lon = center_lon_dist(rng());
    double offset = offset_dist(rng());
    int num_vertices = vertex_count_dist(rng());

    // Generate vertices at equal angular spacing around center (convex polygon)
    std::vector<Coordinate> ring;
    ring.reserve(num_vertices + 1);

    for (int i = 0; i < num_vertices; ++i) {
        double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(num_vertices);
        double lat = center_lat + offset * std::cos(angle);
        double lon = center_lon + offset * std::sin(angle);

        // Clamp to valid ranges
        lat = std::clamp(lat, -90.0, 90.0);
        lon = std::clamp(lon, -180.0, 180.0);

        ring.push_back(Coordinate{lat, lon});
    }

    // Close the ring (first == last)
    ring.push_back(ring.front());

    return Polygon{{std::move(ring)}};
}

Geometry random_geometry() {
    std::uniform_int_distribution<int> type_dist(0, 2);
    switch (type_dist(rng())) {
        case 0:
            return random_point();
        case 1:
            return random_linestring();
        case 2:
        default:
            return random_polygon();
    }
}

PropertyValue random_property_value() {
    std::uniform_int_distribution<int> type_dist(0, 3);
    switch (type_dist(rng())) {
        case 0:
            return PropertyValue{random_alphanumeric()};
        case 1:
            return PropertyValue{random_double()};
        case 2:
            return PropertyValue{random_int64()};
        case 3:
        default:
            return PropertyValue{random_bool()};
    }
}

Properties random_properties() {
    std::uniform_int_distribution<int> count_dist(0, 5);
    int count = count_dist(rng());

    Properties props;
    for (int i = 0; i < count; ++i) {
        props[random_alphanumeric()] = random_property_value();
    }
    return props;
}

GeoFeature random_geo_feature() {
    GeoFeature feature;

    // 50% chance of having an id
    if (random_bool()) {
        feature.id = random_alphanumeric();
    } else {
        feature.id = std::nullopt;
    }

    feature.geometry = random_geometry();
    feature.properties = random_properties();
    return feature;
}

SpatialScale random_scale() {
    return random_bool() ? SpatialScale::Urban : SpatialScale::Regional;
}

Layer random_layer() {
    std::uniform_int_distribution<int> feature_count_dist(0, 10);
    int feature_count = feature_count_dist(rng());

    Layer layer;
    layer.name = random_alphanumeric(1, 20);
    layer.scale = random_scale();

    layer.features.reserve(feature_count);
    for (int i = 0; i < feature_count; ++i) {
        layer.features.push_back(random_geo_feature());
    }
    return layer;
}

BoundingBox random_bounding_box() {
    // Generate valid bbox: south <= north, west <= east
    std::uniform_real_distribution<double> lat_dist(-90.0, 90.0);
    std::uniform_real_distribution<double> lon_dist(-180.0, 180.0);

    double lat1 = lat_dist(rng());
    double lat2 = lat_dist(rng());
    double lon1 = lon_dist(rng());
    double lon2 = lon_dist(rng());

    double south = std::min(lat1, lat2);
    double north = std::max(lat1, lat2);
    double west = std::min(lon1, lon2);
    double east = std::max(lon1, lon2);

    return BoundingBox{
        .south_west = Coordinate{south, west},
        .north_east = Coordinate{north, east},
    };
}

}  // namespace (generators)

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

class MongoPropertyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Generate unique database name per test to avoid collisions
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        std::string db_name = "test_" + std::to_string(dist(rng()));

        connection_string_ = "mongodb://localhost:27017";
        database_name_ = db_name;

        adapter_ = std::make_unique<MongoPersistenceAdapter>(
            connection_string_, database_name_);
    }

    void TearDown() override {
        // Drop the test database to clean up
        adapter_.reset();

        // Reconnect briefly to drop the database
        try {
            MongoPersistenceAdapter cleanup(connection_string_, database_name_);
            // The adapter constructor connects; we just need to trigger cleanup.
            // We'll use a remove on a non-existent layer to verify connection works,
            // then drop via a temporary client. Actually, let's use the mongocxx
            // client directly for cleanup.
        } catch (...) {
            // Best effort cleanup
        }

        // Use a raw mongocxx client to drop the database
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
};

// =============================================================================
// Placeholder: Property tests will be added in tasks 8.2-8.8
// This test verifies the infrastructure compiles and the fixture works.
// =============================================================================

TEST_F(MongoPropertyTest, InfrastructureCompiles) {
    // Verify generators produce valid data
    auto coord = random_coordinate();
    EXPECT_GE(coord.latitude, -90.0);
    EXPECT_LE(coord.latitude, 90.0);
    EXPECT_GE(coord.longitude, -180.0);
    EXPECT_LE(coord.longitude, 180.0);

    auto layer = random_layer();
    EXPECT_FALSE(layer.name.empty());

    auto bbox = random_bounding_box();
    EXPECT_LE(bbox.south_west.latitude, bbox.north_east.latitude);
    EXPECT_LE(bbox.south_west.longitude, bbox.north_east.longitude);
}

// =============================================================================
// Property 1: Layer round-trip preservation
// Validates: Requirements 2.1, 2.4, 2.5, 3.1, 3.4, 3.5, 3.6, 7.1, 7.2, 7.3, 7.5
// =============================================================================

TEST_F(MongoPropertyTest, Property1_RoundTripPreservation) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        auto layer = random_layer();

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value()) << "Iteration " << i << ": save_layer failed";

        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_TRUE(find_result.has_value()) << "Iteration " << i << ": find_layer failed";

        EXPECT_TRUE(layer_equal(layer, find_result.value()))
            << "Iteration " << i << ": layer mismatch for '" << layer.name << "'";

        // Clean up for next iteration (avoid duplicate name collisions)
        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Property 2: Duplicate layer detection
// Validates: Requirements 2.2
// =============================================================================

TEST_F(MongoPropertyTest, Property2_DuplicateLayerDetection) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        auto layer = random_layer();

        auto first_save = adapter_->save_layer(layer);
        ASSERT_TRUE(first_save.has_value()) << "Iteration " << i << ": first save_layer failed";

        // Second save with same name should return DuplicateLayer
        auto second_save = adapter_->save_layer(layer);
        ASSERT_FALSE(second_save.has_value()) << "Iteration " << i << ": second save should fail";
        EXPECT_EQ(second_save.error(), PersistenceError::DuplicateLayer)
            << "Iteration " << i << ": expected DuplicateLayer error";

        // Clean up for next iteration
        adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Property 5: Remove makes layer unfindable
// Validates: Requirements 5.1, 5.4
// =============================================================================

TEST_F(MongoPropertyTest, Property5_RemoveMakesLayerUnfindable) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        auto layer = random_layer();

        // Save the layer
        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value()) << "Iteration " << i << ": save_layer failed";

        // Remove it
        auto remove_result = adapter_->remove_layer(layer.name);
        ASSERT_TRUE(remove_result.has_value()) << "Iteration " << i << ": remove_layer failed";

        // Find should now return NotFound
        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_FALSE(find_result.has_value()) << "Iteration " << i << ": find_layer should fail after remove";
        EXPECT_EQ(find_result.error(), PersistenceError::NotFound)
            << "Iteration " << i << ": expected NotFound error after remove";
    }
}

// =============================================================================
// Property 4: List layers reflects saved state
// Validates: Requirements 4.1
// =============================================================================

TEST_F(MongoPropertyTest, Property4_ListLayersReflectsSavedState) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        // Generate 1-5 layers with guaranteed distinct names
        std::uniform_int_distribution<int> count_dist(1, 5);
        int n = count_dist(rng());

        std::vector<std::string> expected_names;
        for (int j = 0; j < n; ++j) {
            auto layer = random_layer();
            // Ensure distinct name by appending index
            layer.name = layer.name + "_" + std::to_string(i) + "_" + std::to_string(j);

            auto save_result = adapter_->save_layer(layer);
            ASSERT_TRUE(save_result.has_value())
                << "Iteration " << i << ": save_layer failed for '" << layer.name << "'"
                << " error: " << static_cast<int>(save_result.error());
            expected_names.push_back(layer.name);
        }

        auto list_result = adapter_->list_layers();
        ASSERT_TRUE(list_result.has_value()) << "Iteration " << i << ": list_layers failed";

        auto listed = list_result.value();
        std::sort(listed.begin(), listed.end());
        std::sort(expected_names.begin(), expected_names.end());
        EXPECT_EQ(listed, expected_names) << "Iteration " << i << ": listed names don't match";

        // Clean up for next iteration
        for (const auto& name : expected_names) {
            adapter_->remove_layer(name);
        }
    }
}

// =============================================================================
// Property 3: Find non-existent layer returns NotFound
// Validates: Requirements 3.2
// =============================================================================

TEST_F(MongoPropertyTest, Property3_FindNonExistentReturnsNotFound) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        // Generate a random name that has never been saved
        std::string name = random_alphanumeric(1, 20);

        auto result = adapter_->find_layer(name);
        ASSERT_FALSE(result.has_value()) << "Iteration " << i << ": find_layer should fail for '" << name << "'";
        EXPECT_EQ(result.error(), PersistenceError::NotFound)
            << "Iteration " << i << ": expected NotFound error";
    }
}

// =============================================================================
// Property 6: Remove non-existent layer returns NotFound
// Validates: Requirements 5.2
// =============================================================================

TEST_F(MongoPropertyTest, Property6_RemoveNonExistentReturnsNotFound) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        // Generate a random name that has never been saved
        std::string name = random_alphanumeric(1, 20);

        auto result = adapter_->remove_layer(name);
        ASSERT_FALSE(result.has_value()) << "Iteration " << i << ": remove_layer should fail for '" << name << "'";
        EXPECT_EQ(result.error(), PersistenceError::NotFound)
            << "Iteration " << i << ": expected NotFound error";
    }
}

// =============================================================================
// Property 7: Spatial query correctness
// Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.6
// =============================================================================

namespace {

// Helper: check if a point is inside the bounding box
bool point_in_bbox(const Coordinate& point, const BoundingBox& bbox) {
    return point.latitude >= bbox.south_west.latitude &&
           point.latitude <= bbox.north_east.latitude &&
           point.longitude >= bbox.south_west.longitude &&
           point.longitude <= bbox.north_east.longitude;
}

// Helper: generate a small bounding box (0.5 to 3 degrees extent)
BoundingBox random_small_bounding_box() {
    // Center the bbox in safe ranges to avoid edge wrapping
    std::uniform_real_distribution<double> center_lat_dist(-70.0, 70.0);
    std::uniform_real_distribution<double> center_lon_dist(-160.0, 160.0);
    std::uniform_real_distribution<double> extent_dist(0.5, 3.0);

    double center_lat = center_lat_dist(rng());
    double center_lon = center_lon_dist(rng());
    double lat_extent = extent_dist(rng());
    double lon_extent = extent_dist(rng());

    double south = std::clamp(center_lat - lat_extent / 2.0, -90.0, 90.0);
    double north = std::clamp(center_lat + lat_extent / 2.0, -90.0, 90.0);
    double west = std::clamp(center_lon - lon_extent / 2.0, -180.0, 180.0);
    double east = std::clamp(center_lon + lon_extent / 2.0, -180.0, 180.0);

    return BoundingBox{
        .south_west = Coordinate{south, west},
        .north_east = Coordinate{north, east},
    };
}

// Helper: generate a GeoFeature with a Point geometry inside the given bbox
GeoFeature feature_inside_bbox(const BoundingBox& bbox) {
    // Place point well inside the bbox (use 20%-80% of extent)
    std::uniform_real_distribution<double> frac_dist(0.2, 0.8);
    double lat = bbox.south_west.latitude +
                 frac_dist(rng()) * (bbox.north_east.latitude - bbox.south_west.latitude);
    double lon = bbox.south_west.longitude +
                 frac_dist(rng()) * (bbox.north_east.longitude - bbox.south_west.longitude);

    GeoFeature feature;
    feature.id = "inside_" + random_alphanumeric(1, 10);
    feature.geometry = Point{Coordinate{lat, lon}};
    feature.properties = random_properties();
    return feature;
}

// Helper: generate a GeoFeature with a Point geometry far outside the given bbox
GeoFeature feature_outside_bbox(const BoundingBox& bbox) {
    // Place point at least 10 degrees away from bbox in latitude
    double lat;
    if (bbox.north_east.latitude + 10.0 <= 90.0) {
        lat = bbox.north_east.latitude + 10.0;
    } else if (bbox.south_west.latitude - 10.0 >= -90.0) {
        lat = bbox.south_west.latitude - 10.0;
    } else {
        // Fallback: go above
        lat = std::clamp(bbox.north_east.latitude + 10.0, -90.0, 90.0);
    }

    // Keep longitude in range
    std::uniform_real_distribution<double> lon_dist(-180.0, 180.0);
    double lon = lon_dist(rng());

    GeoFeature feature;
    feature.id = "outside_" + random_alphanumeric(1, 10);
    feature.geometry = Point{Coordinate{lat, lon}};
    feature.properties = random_properties();
    return feature;
}

}  // namespace (spatial helpers)

TEST_F(MongoPropertyTest, Property7_SpatialQueryCorrectness) {
    constexpr int kIterations = 100;
    for (int i = 0; i < kIterations; ++i) {
        // Generate a small bounding box
        auto bbox = random_small_bounding_box();

        // Generate features deliberately inside and outside
        std::uniform_int_distribution<int> inside_count_dist(1, 3);
        std::uniform_int_distribution<int> outside_count_dist(1, 3);
        int num_inside = inside_count_dist(rng());
        int num_outside = outside_count_dist(rng());

        Layer layer;
        layer.name = "spatial_test_" + std::to_string(i) + "_" + random_alphanumeric(1, 8);
        layer.scale = random_scale();

        std::vector<std::string> expected_inside_ids;
        for (int j = 0; j < num_inside; ++j) {
            auto f = feature_inside_bbox(bbox);
            expected_inside_ids.push_back(f.id.value());
            layer.features.push_back(std::move(f));
        }

        std::vector<std::string> expected_outside_ids;
        for (int j = 0; j < num_outside; ++j) {
            auto f = feature_outside_bbox(bbox);
            expected_outside_ids.push_back(f.id.value());
            layer.features.push_back(std::move(f));
        }

        // Save the layer
        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Iteration " << i << ": save_layer failed";

        // Query with the bounding box
        auto query_result = adapter_->query_features(bbox);
        ASSERT_TRUE(query_result.has_value())
            << "Iteration " << i << ": query_features failed";

        auto& returned_features = query_result.value();

        // Verify: all returned features exist in the saved layer (no spurious results)
        for (const auto& rf : returned_features) {
            bool found_in_layer = false;
            for (const auto& lf : layer.features) {
                if (feature_equal(rf, lf)) {
                    found_in_layer = true;
                    break;
                }
            }
            EXPECT_TRUE(found_in_layer)
                << "Iteration " << i << ": returned feature not found in saved layer";
        }

        // Verify: all Point features placed inside the bbox ARE returned
        for (const auto& inside_id : expected_inside_ids) {
            bool found = false;
            for (const auto& rf : returned_features) {
                if (rf.id.has_value() && rf.id.value() == inside_id) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found)
                << "Iteration " << i << ": inside feature '" << inside_id << "' not returned";
        }

        // Verify: Point features placed far outside are NOT returned
        for (const auto& outside_id : expected_outside_ids) {
            bool found = false;
            for (const auto& rf : returned_features) {
                if (rf.id.has_value() && rf.id.value() == outside_id) {
                    found = true;
                    break;
                }
            }
            EXPECT_FALSE(found)
                << "Iteration " << i << ": outside feature '" << outside_id << "' was returned";
        }

        // Clean up
        (void)adapter_->remove_layer(layer.name);
    }

    // --- Test inverted bounding box returns empty vector ---
    // Save a layer with a feature, then query with inverted bbox
    {
        Layer layer;
        layer.name = "inverted_bbox_test_" + random_alphanumeric(1, 8);
        layer.scale = SpatialScale::Urban;
        GeoFeature f;
        f.id = "test_feature";
        f.geometry = Point{Coordinate{0.0, 0.0}};
        layer.features.push_back(std::move(f));

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value()) << "inverted bbox test: save_layer failed";

        // Inverted latitude (south > north)
        BoundingBox inverted_lat{
            .south_west = Coordinate{10.0, -10.0},
            .north_east = Coordinate{-10.0, 10.0},
        };
        auto result_lat = adapter_->query_features(inverted_lat);
        ASSERT_TRUE(result_lat.has_value()) << "inverted bbox test (lat): query_features failed";
        EXPECT_TRUE(result_lat.value().empty())
            << "inverted bbox (lat): should return empty vector";

        // Inverted longitude (west > east)
        BoundingBox inverted_lon{
            .south_west = Coordinate{-10.0, 10.0},
            .north_east = Coordinate{10.0, -10.0},
        };
        auto result_lon = adapter_->query_features(inverted_lon);
        ASSERT_TRUE(result_lon.has_value()) << "inverted bbox test (lon): query_features failed";
        EXPECT_TRUE(result_lon.value().empty())
            << "inverted bbox (lon): should return empty vector";

        // Clean up
        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Preservation Property: Empty name → WriteError
// Validates: Requirements 3.1
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_EmptyNameReturnsWriteError) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        // Generate a layer with empty name but valid random features/geometry
        auto layer = random_layer();
        layer.name = "";  // Force empty name

        auto save_result = adapter_->save_layer(layer);
        ASSERT_FALSE(save_result.has_value())
            << "Iteration " << i << ": save_layer with empty name should fail";
        EXPECT_EQ(save_result.error(), PersistenceError::WriteError)
            << "Iteration " << i << ": expected WriteError for empty name";
    }
}

// =============================================================================
// Preservation Property: Empty features vector → persist and retrieve
// Validates: Requirements 3.3
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_EmptyFeaturesLayerPersists) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        // Generate a layer with valid name but empty features
        Layer layer;
        layer.name = random_alphanumeric(1, 20);
        layer.scale = random_scale();
        layer.features.clear();  // Explicitly empty

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Iteration " << i << ": save_layer with empty features should succeed";

        // Verify it can be retrieved
        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_TRUE(find_result.has_value())
            << "Iteration " << i << ": find_layer should succeed after saving empty-features layer";

        EXPECT_EQ(find_result.value().name, layer.name)
            << "Iteration " << i << ": retrieved layer name mismatch";
        EXPECT_TRUE(find_result.value().features.empty())
            << "Iteration " << i << ": retrieved layer should have empty features";

        // Clean up
        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Preservation Property: Empty database → list_layers returns empty vector
// Validates: Requirements 3.4
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_EmptyDatabaseListLayersReturnsEmpty) {
    // On a fresh database (from SetUp), list_layers should return empty
    auto list_result = adapter_->list_layers();
    ASSERT_TRUE(list_result.has_value())
        << "list_layers on empty database should not error";
    EXPECT_TRUE(list_result.value().empty())
        << "list_layers on empty database should return empty vector, got "
        << list_result.value().size() << " items";
}

// =============================================================================
// Preservation Property: Point geometry round-trips correctly
// Validates: Requirements 3.5, 3.6
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_PointGeometryRoundTrip) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        Layer layer;
        layer.name = "point_rt_" + std::to_string(i) + "_" + random_alphanumeric(1, 8);
        layer.scale = random_scale();

        GeoFeature feature;
        feature.id = "pt_" + std::to_string(i);
        feature.geometry = random_point();
        feature.properties = random_properties();
        layer.features.push_back(feature);

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Iteration " << i << ": save_layer with Point geometry failed";

        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_TRUE(find_result.has_value())
            << "Iteration " << i << ": find_layer failed for Point layer";

        ASSERT_EQ(find_result.value().features.size(), 1u)
            << "Iteration " << i << ": expected 1 feature";

        EXPECT_TRUE(geometry_equal(feature.geometry, find_result.value().features[0].geometry))
            << "Iteration " << i << ": Point geometry mismatch after round-trip";

        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Preservation Property: LineString geometry round-trips correctly
// Validates: Requirements 3.5, 3.6
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_LineStringGeometryRoundTrip) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        Layer layer;
        layer.name = "ls_rt_" + std::to_string(i) + "_" + random_alphanumeric(1, 8);
        layer.scale = random_scale();

        GeoFeature feature;
        feature.id = "ls_" + std::to_string(i);
        feature.geometry = random_linestring();
        feature.properties = random_properties();
        layer.features.push_back(feature);

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Iteration " << i << ": save_layer with LineString geometry failed";

        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_TRUE(find_result.has_value())
            << "Iteration " << i << ": find_layer failed for LineString layer";

        ASSERT_EQ(find_result.value().features.size(), 1u)
            << "Iteration " << i << ": expected 1 feature";

        EXPECT_TRUE(geometry_equal(feature.geometry, find_result.value().features[0].geometry))
            << "Iteration " << i << ": LineString geometry mismatch after round-trip";

        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Preservation Property: Polygon geometry round-trips correctly
// Validates: Requirements 3.5, 3.6
// =============================================================================

TEST_F(MongoPropertyTest, Preservation_PolygonGeometryRoundTrip) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        Layer layer;
        layer.name = "poly_rt_" + std::to_string(i) + "_" + random_alphanumeric(1, 8);
        layer.scale = random_scale();

        GeoFeature feature;
        feature.id = "poly_" + std::to_string(i);
        feature.geometry = random_polygon();
        feature.properties = random_properties();
        layer.features.push_back(feature);

        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Iteration " << i << ": save_layer with Polygon geometry failed";

        auto find_result = adapter_->find_layer(layer.name);
        ASSERT_TRUE(find_result.has_value())
            << "Iteration " << i << ": find_layer failed for Polygon layer";

        ASSERT_EQ(find_result.value().features.size(), 1u)
            << "Iteration " << i << ": expected 1 feature";

        EXPECT_TRUE(geometry_equal(feature.geometry, find_result.value().features[0].geometry))
            << "Iteration " << i << ": Polygon geometry mismatch after round-trip";

        (void)adapter_->remove_layer(layer.name);
    }
}

// =============================================================================
// Bug Condition Exploration: save_layer success implies persistence
// Validates: Requirements 1.1, 1.3, 1.4
//
// This test encodes the EXPECTED behavior: if save_layer() reports success,
// the document MUST actually be persisted (list_layers() contains the name,
// find_layer() retrieves the layer).
//
// On UNFIXED code in CI (MongoDB 7 with index issues), this test is expected
// to FAIL because save_layer() returns success but insert_one() silently fails.
// =============================================================================

TEST_F(MongoPropertyTest, BugCondition_SaveLayerSuccessImpliesPersistence) {
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        auto layer = random_layer();

        // Ensure the layer has at least one feature with geometry to exercise
        // the 2dsphere index path (where the bug manifests)
        if (layer.features.empty()) {
            GeoFeature f;
            f.id = "bug_probe_" + std::to_string(i);
            f.geometry = random_geometry();
            f.properties = random_properties();
            layer.features.push_back(std::move(f));
        }

        auto save_result = adapter_->save_layer(layer);

        // We only assert on the persistence guarantee IF save_layer() reports success.
        // The bug condition is: save_layer() returns success but the document isn't persisted.
        if (save_result.has_value()) {
            // Property: save_layer() success ⟹ list_layers() contains the layer name
            auto list_result = adapter_->list_layers();
            ASSERT_TRUE(list_result.has_value())
                << "Iteration " << i << ": list_layers() failed after successful save";

            const auto& names = list_result.value();
            bool found_in_list = std::find(names.begin(), names.end(), layer.name) != names.end();
            EXPECT_TRUE(found_in_list)
                << "Iteration " << i << ": save_layer() returned success for '"
                << layer.name << "' but list_layers() does not contain it. "
                << "list_layers() returned " << names.size() << " names. "
                << "BUG CONDITION: save reports success but document not persisted.";

            // Property: save_layer() success ⟹ find_layer(name) retrieves the layer
            auto find_result = adapter_->find_layer(layer.name);
            EXPECT_TRUE(find_result.has_value())
                << "Iteration " << i << ": save_layer() returned success for '"
                << layer.name << "' but find_layer() returned error. "
                << "BUG CONDITION: save reports success but document not persisted.";

            if (find_result.has_value()) {
                EXPECT_EQ(find_result.value().name, layer.name)
                    << "Iteration " << i << ": retrieved layer name mismatch";
            }
        }

        // Clean up for next iteration
        (void)adapter_->remove_layer(layer.name);
    }
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
