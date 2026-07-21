#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/core/domain/bounding_box.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"
#include "src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {
namespace {

using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::SpatialScale;
using core::ports::PersistenceError;

/// Test fixture that creates a unique temp directory and cleans it up.
class FilePersistenceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a unique temp directory for this test
        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("fp_test_" + std::to_string(
                         std::hash<std::string>{}(
                             ::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name())));
        // Ensure clean state
        std::filesystem::remove_all(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
};

// --- Test 1: SaveThenFindReturnsEquivalentLayer ---

TEST_F(FilePersistenceTest, SaveThenFindReturnsEquivalentLayer) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer;
    layer.name = "bilbao_stops";
    layer.scale = SpatialScale::Urban;
    layer.features = {
        GeoFeature{
            .id = "stop_001",
            .geometry = Point{{43.2630, -2.9350}},
            .properties = {{"name", std::string("Plaza Moyua")},
                           {"lines", int64_t{5}},
                           {"accessible", true}},
        },
        GeoFeature{
            .id = "stop_002",
            .geometry = Point{{43.2600, -2.9400}},
            .properties = {{"name", std::string("Abando")}},
        },
    };

    auto save_result = adapter.save_layer(layer);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter.find_layer("bilbao_stops");
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    const auto& found = find_result.value();
    EXPECT_EQ(found.name, layer.name);
    EXPECT_EQ(found.scale, layer.scale);
    EXPECT_EQ(found.features.size(), layer.features.size());
}

// --- Test 2: FindNonExistentReturnsNotFound ---

TEST_F(FilePersistenceTest, FindNonExistentReturnsNotFound) {
    FilePersistenceAdapter adapter(temp_dir_);

    auto result = adapter.find_layer("nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PersistenceError::NotFound);
}

// --- Test 3: ListLayersReturnsNames ---

TEST_F(FilePersistenceTest, ListLayersReturnsNames) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer1{.name = "layer_alpha",
                 .scale = SpatialScale::Urban,
                 .features = {}};
    Layer layer2{.name = "layer_beta",
                 .scale = SpatialScale::Regional,
                 .features = {}};

    ASSERT_TRUE(adapter.save_layer(layer1).has_value());
    ASSERT_TRUE(adapter.save_layer(layer2).has_value());

    auto result = adapter.list_layers();
    ASSERT_TRUE(result.has_value());

    auto& names = result.value();
    EXPECT_EQ(names.size(), 2u);

    // Sort for deterministic comparison (directory iteration order is unspecified)
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "layer_alpha");
    EXPECT_EQ(names[1], "layer_beta");
}

// --- Test 4: RemoveThenFindReturnsNotFound ---

TEST_F(FilePersistenceTest, RemoveThenFindReturnsNotFound) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer{.name = "to_remove",
                .scale = SpatialScale::Urban,
                .features = {}};

    ASSERT_TRUE(adapter.save_layer(layer).has_value());

    auto remove_result = adapter.remove_layer("to_remove");
    ASSERT_TRUE(remove_result.has_value()) << "remove_layer failed";

    auto find_result = adapter.find_layer("to_remove");
    ASSERT_FALSE(find_result.has_value());
    EXPECT_EQ(find_result.error(), PersistenceError::NotFound);
}

// --- Test 5: QueryFeaturesPointInside ---

TEST_F(FilePersistenceTest, QueryFeaturesPointInside) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer{
        .name = "query_test",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "inside",
                .geometry = Point{{43.26, -2.93}},
                .properties = {},
            },
        },
    };

    ASSERT_TRUE(adapter.save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter.query_features(bbox);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].id.value(), "inside");
}

// --- Test 6: QueryFeaturesPointOutside ---

TEST_F(FilePersistenceTest, QueryFeaturesPointOutside) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer{
        .name = "query_outside",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "outside",
                .geometry = Point{{40.0, -3.7}},
                .properties = {},
            },
        },
    };

    ASSERT_TRUE(adapter.save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter.query_features(bbox);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

// --- Test 7: EnsureDirectoryCreated ---

TEST_F(FilePersistenceTest, EnsureDirectoryCreated) {
    // Use a nested path that does not exist yet
    auto nested_dir = temp_dir_ / "nested" / "data" / "dir";
    ASSERT_FALSE(std::filesystem::exists(nested_dir));

    FilePersistenceAdapter adapter(nested_dir);

    EXPECT_TRUE(std::filesystem::exists(nested_dir));
    EXPECT_TRUE(std::filesystem::is_directory(nested_dir));
}

// --- Test 8: SaveDuplicateReturnsDuplicateLayer ---

TEST_F(FilePersistenceTest, SaveDuplicateReturnsDuplicateLayer) {
    FilePersistenceAdapter adapter(temp_dir_);

    Layer layer{.name = "duplicate_test",
                .scale = SpatialScale::Urban,
                .features = {}};

    auto first = adapter.save_layer(layer);
    ASSERT_TRUE(first.has_value());

    auto second = adapter.save_layer(layer);
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), PersistenceError::DuplicateLayer);
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
