#include <gtest/gtest.h>

#include "../src/core/domain/bounding_box.h"
#include "../src/core/domain/coordinate.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/layer.h"

namespace garraiobide::core::domain {
namespace {

// --- Coordinate tests ---

TEST(CoordinateTest, Equality) {
    Coordinate a{43.2630, -2.9350};
    Coordinate b{43.2630, -2.9350};
    EXPECT_EQ(a, b);
}

TEST(CoordinateTest, Inequality) {
    Coordinate a{43.2630, -2.9350};
    Coordinate b{43.2631, -2.9350};
    EXPECT_NE(a, b);
}

// --- BoundingBox tests ---

class BoundingBoxTest : public ::testing::Test {
   protected:
    // Bilbao area bounding box
    BoundingBox bilbao_bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };
};

TEST_F(BoundingBoxTest, ContainsPointInside) {
    Coordinate plaza_moyua{43.2630, -2.9350};
    EXPECT_TRUE(bilbao_bbox.contains(plaza_moyua));
}

TEST_F(BoundingBoxTest, DoesNotContainPointOutside) {
    Coordinate donostia{43.3183, -1.9812};
    EXPECT_FALSE(bilbao_bbox.contains(donostia));
}

TEST_F(BoundingBoxTest, ContainsPointOnBorder) {
    Coordinate on_edge{43.23, -2.95};
    EXPECT_TRUE(bilbao_bbox.contains(on_edge));
}

TEST_F(BoundingBoxTest, IntersectsOverlapping) {
    BoundingBox overlap{
        .south_west = {43.25, -2.95},
        .north_east = {43.35, -2.85},
    };
    EXPECT_TRUE(bilbao_bbox.intersects(overlap));
}

TEST_F(BoundingBoxTest, DoesNotIntersectDisjoint) {
    BoundingBox far_away{
        .south_west = {40.0, -3.7},
        .north_east = {40.5, -3.6},
    };
    EXPECT_FALSE(bilbao_bbox.intersects(far_away));
}

// --- Layer envelope tests ---

TEST(LayerTest, EnvelopeOfEmptyLayerIsNullopt) {
    Layer layer{.name = "empty", .scale = SpatialScale::Urban};
    EXPECT_FALSE(layer.envelope().has_value());
}

TEST(LayerTest, EnvelopeSinglePoint) {
    GeoFeature feature{
        .id = "1",
        .geometry = Point{{43.2630, -2.9350}},
        .properties = {},
    };
    Layer layer{
        .name = "single",
        .scale = SpatialScale::Urban,
        .features = {feature},
    };

    auto env = layer.envelope();
    ASSERT_TRUE(env.has_value());
    EXPECT_DOUBLE_EQ(env->south_west.latitude, 43.2630);
    EXPECT_DOUBLE_EQ(env->south_west.longitude, -2.9350);
    EXPECT_DOUBLE_EQ(env->north_east.latitude, 43.2630);
    EXPECT_DOUBLE_EQ(env->north_east.longitude, -2.9350);
}

TEST(LayerTest, EnvelopeMultiplePoints) {
    GeoFeature f1{
        .id = "1",
        .geometry = Point{{43.25, -2.95}},
        .properties = {},
    };
    GeoFeature f2{
        .id = "2",
        .geometry = Point{{43.28, -2.92}},
        .properties = {},
    };
    Layer layer{
        .name = "multi",
        .scale = SpatialScale::Regional,
        .features = {f1, f2},
    };

    auto env = layer.envelope();
    ASSERT_TRUE(env.has_value());
    EXPECT_DOUBLE_EQ(env->south_west.latitude, 43.25);
    EXPECT_DOUBLE_EQ(env->south_west.longitude, -2.95);
    EXPECT_DOUBLE_EQ(env->north_east.latitude, 43.28);
    EXPECT_DOUBLE_EQ(env->north_east.longitude, -2.92);
}

TEST(LayerTest, EnvelopeIncludesLineString) {
    GeoFeature f{
        .id = "line1",
        .geometry = LineString{{
            {43.25, -2.95},
            {43.28, -2.90},
            {43.26, -2.98},
        }},
        .properties = {},
    };
    Layer layer{
        .name = "lines",
        .scale = SpatialScale::Urban,
        .features = {f},
    };

    auto env = layer.envelope();
    ASSERT_TRUE(env.has_value());
    EXPECT_DOUBLE_EQ(env->south_west.latitude, 43.25);
    EXPECT_DOUBLE_EQ(env->south_west.longitude, -2.98);
    EXPECT_DOUBLE_EQ(env->north_east.latitude, 43.28);
    EXPECT_DOUBLE_EQ(env->north_east.longitude, -2.90);
}

}  // namespace
}  // namespace garraiobide::core::domain
