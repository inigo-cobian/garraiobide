#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

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

// --- Point serialization ---

TEST(GeoJsonSerializerTest, PointSerializationSwapsCoordinates) {
    GeoFeature feature{
        .id = "pt1",
        .geometry = Point{{43.2630, -2.9350}},
        .properties = {},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_EQ(result["geometry"]["type"], "Point");
    auto coords = result["geometry"]["coordinates"];
    EXPECT_DOUBLE_EQ(coords[0].get<double>(), -2.9350);
    EXPECT_DOUBLE_EQ(coords[1].get<double>(), 43.2630);
}

// --- LineString serialization ---

TEST(GeoJsonSerializerTest, LineStringSerializationProducesArrayOfLngLat) {
    GeoFeature feature{
        .id = "line1",
        .geometry = LineString{{{43.25, -2.95}, {43.28, -2.90}}},
        .properties = {},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_EQ(result["geometry"]["type"], "LineString");
    auto coords = result["geometry"]["coordinates"];
    ASSERT_EQ(coords.size(), 2u);
    EXPECT_DOUBLE_EQ(coords[0][0].get<double>(), -2.95);
    EXPECT_DOUBLE_EQ(coords[0][1].get<double>(), 43.25);
    EXPECT_DOUBLE_EQ(coords[1][0].get<double>(), -2.90);
    EXPECT_DOUBLE_EQ(coords[1][1].get<double>(), 43.28);
}

// --- Polygon serialization ---

TEST(GeoJsonSerializerTest, PolygonSerializationProducesNestedArrays) {
    // A simple triangle ring
    std::vector<Coordinate> ring = {
        {43.25, -2.95},
        {43.28, -2.90},
        {43.26, -2.98},
        {43.25, -2.95},  // closed ring
    };

    GeoFeature feature{
        .id = "poly1",
        .geometry = Polygon{{ring}},
        .properties = {},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_EQ(result["geometry"]["type"], "Polygon");
    auto coords = result["geometry"]["coordinates"];
    ASSERT_EQ(coords.size(), 1u);  // one ring
    ASSERT_EQ(coords[0].size(), 4u);
    // First vertex: lat=43.25, lng=-2.95 → [lng, lat] = [-2.95, 43.25]
    EXPECT_DOUBLE_EQ(coords[0][0][0].get<double>(), -2.95);
    EXPECT_DOUBLE_EQ(coords[0][0][1].get<double>(), 43.25);
    // Last vertex closes the ring
    EXPECT_DOUBLE_EQ(coords[0][3][0].get<double>(), -2.95);
    EXPECT_DOUBLE_EQ(coords[0][3][1].get<double>(), 43.25);
}

// --- Empty layer ---

TEST(GeoJsonSerializerTest, EmptyLayerProducesEmptyFeatureCollection) {
    Layer layer{.name = "empty", .scale = SpatialScale::Urban, .features = {}};

    auto result = json::parse(GeoJsonSerializer::serialize_layer(layer));

    EXPECT_EQ(result["type"], "FeatureCollection");
    EXPECT_TRUE(result["features"].is_array());
    EXPECT_EQ(result["features"].size(), 0u);
}

// --- Feature without id ---

TEST(GeoJsonSerializerTest, FeatureWithoutIdOmitsIdField) {
    GeoFeature feature{
        .id = std::nullopt,
        .geometry = Point{{43.2630, -2.9350}},
        .properties = {},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_EQ(result["type"], "Feature");
    EXPECT_FALSE(result.contains("id"));
}

// --- Feature with id ---

TEST(GeoJsonSerializerTest, FeatureWithIdIncludesIdField) {
    GeoFeature feature{
        .id = "stop_001",
        .geometry = Point{{43.2630, -2.9350}},
        .properties = {},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_EQ(result["type"], "Feature");
    EXPECT_EQ(result["id"], "stop_001");
}

// --- PropertyValue types ---

TEST(GeoJsonSerializerTest, StringPropertySerializesToJsonString) {
    GeoFeature feature{
        .id = "f1",
        .geometry = Point{{43.26, -2.93}},
        .properties = {{"name", std::string("Plaza Moyua")}},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_TRUE(result["properties"]["name"].is_string());
    EXPECT_EQ(result["properties"]["name"], "Plaza Moyua");
}

TEST(GeoJsonSerializerTest, DoublePropertySerializesToJsonNumber) {
    GeoFeature feature{
        .id = "f2",
        .geometry = Point{{43.26, -2.93}},
        .properties = {{"rating", 4.5}},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_TRUE(result["properties"]["rating"].is_number_float());
    EXPECT_DOUBLE_EQ(result["properties"]["rating"].get<double>(), 4.5);
}

TEST(GeoJsonSerializerTest, Int64PropertySerializesToJsonInteger) {
    GeoFeature feature{
        .id = "f3",
        .geometry = Point{{43.26, -2.93}},
        .properties = {{"lines", int64_t{5}}},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_TRUE(result["properties"]["lines"].is_number_integer());
    EXPECT_EQ(result["properties"]["lines"].get<int64_t>(), 5);
}

TEST(GeoJsonSerializerTest, BoolPropertySerializesToJsonBoolean) {
    GeoFeature feature{
        .id = "f4",
        .geometry = Point{{43.26, -2.93}},
        .properties = {{"accessible", true}},
    };

    auto result = json::parse(GeoJsonSerializer::serialize_feature(feature));

    EXPECT_TRUE(result["properties"]["accessible"].is_boolean());
    EXPECT_EQ(result["properties"]["accessible"].get<bool>(), true);
}

}  // namespace
}  // namespace garraiobide::adapters::http
