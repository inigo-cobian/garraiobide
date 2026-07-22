#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include "src/adapters/http/geojson_serializer.h"
#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"
#include "src/core/domain/properties.h"

// =============================================================================
// Property 3: End-to-end coordinate consistency (persistence to HTTP output)
//
// For any valid Layer object, when the File Persistence Adapter saves and
// reloads it, and then the GeoJSON serializer serializes the result, every
// coordinate array in the HTTP output SHALL be in [longitude, latitude] order
// without any intermediate transformation or coordinate swap being applied
// between the two adapters.
//
// **Validates: Requirements 3.1**
// =============================================================================

namespace garraiobide {
namespace {

using nlohmann::json;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::Properties;
using core::domain::SpatialScale;
using adapters::http::GeoJsonSerializer;
using adapters::persistence::FilePersistenceAdapter;

// --- Test fixture with temp directory management ---

class E2ECoordinateConsistencyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        auto base = std::filesystem::temp_directory_path();
        temp_dir_ = base / ("e2e_coord_test_" + std::to_string(
                                                     std::random_device{}()));
        std::filesystem::create_directories(temp_dir_);
        adapter_ = std::make_unique<FilePersistenceAdapter>(temp_dir_);
    }

    void TearDown() override {
        adapter_.reset();
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    std::filesystem::path temp_dir_;
    std::unique_ptr<FilePersistenceAdapter> adapter_;
};

// --- Helper: verify that a JSON coordinate array is [longitude, latitude] ---

void assert_coordinate_order(const json& coord_array,
                             double expected_lon,
                             double expected_lat,
                             const std::string& context) {
    ASSERT_TRUE(coord_array.is_array()) << context << ": not an array";
    ASSERT_EQ(coord_array.size(), 2u) << context << ": expected 2 elements";
    EXPECT_DOUBLE_EQ(coord_array[0].get<double>(), expected_lon)
        << context << ": coord[0] should be longitude";
    EXPECT_DOUBLE_EQ(coord_array[1].get<double>(), expected_lat)
        << context << ": coord[1] should be latitude";
}

// =============================================================================
// Test 1: Known asymmetric coordinates through the full pipeline
// Uses lat=43.2630, lon=-2.9350 (Bilbao) — deliberately asymmetric so that
// any swap is immediately detectable.
// =============================================================================

TEST_F(E2ECoordinateConsistencyTest, AllGeometryTypes_AsymmetricCoords) {
    const double lat = 43.2630;
    const double lon = -2.9350;

    Layer layer{
        .name = "e2e_all_geom",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "point_feat",
                .geometry = Point{{lat, lon}},
                .properties = {{"type", std::string("stop")}},
            },
            GeoFeature{
                .id = "line_feat",
                .geometry = LineString{{{lat, lon}, {lat + 0.01, lon - 0.01}}},
                .properties = {{"type", std::string("route")}},
            },
            GeoFeature{
                .id = "poly_feat",
                .geometry = Polygon{{
                    {{lat, lon},
                     {lat, lon + 0.02},
                     {lat + 0.02, lon + 0.02},
                     {lat + 0.02, lon},
                     {lat, lon}},
                }},
                .properties = {{"type", std::string("zone")}},
            },
        },
    };

    // Save -> Load -> Serialize
    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter_->find_layer("e2e_all_geom");
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    std::string geojson_str =
        GeoJsonSerializer::serialize_layer(find_result.value());
    json geojson = json::parse(geojson_str);

    ASSERT_EQ(geojson["type"], "FeatureCollection");
    const auto& features = geojson["features"];
    ASSERT_EQ(features.size(), 3u);

    // Verify Point: coordinates = [lon, lat]
    {
        const auto& geom = features[0]["geometry"];
        ASSERT_EQ(geom["type"], "Point");
        assert_coordinate_order(geom["coordinates"], lon, lat, "Point");
    }

    // Verify LineString: each vertex = [lon, lat]
    {
        const auto& geom = features[1]["geometry"];
        ASSERT_EQ(geom["type"], "LineString");
        const auto& coords = geom["coordinates"];
        ASSERT_EQ(coords.size(), 2u);
        assert_coordinate_order(coords[0], lon, lat, "LineString[0]");
        assert_coordinate_order(coords[1], lon - 0.01, lat + 0.01,
                                "LineString[1]");
    }

    // Verify Polygon: each ring vertex = [lon, lat]
    {
        const auto& geom = features[2]["geometry"];
        ASSERT_EQ(geom["type"], "Polygon");
        const auto& rings = geom["coordinates"];
        ASSERT_EQ(rings.size(), 1u);
        const auto& ring = rings[0];
        ASSERT_EQ(ring.size(), 5u);
        assert_coordinate_order(ring[0], lon, lat, "Polygon ring[0]");
        assert_coordinate_order(ring[1], lon + 0.02, lat, "Polygon ring[1]");
        assert_coordinate_order(ring[2], lon + 0.02, lat + 0.02,
                                "Polygon ring[2]");
        assert_coordinate_order(ring[3], lon, lat + 0.02, "Polygon ring[3]");
        assert_coordinate_order(ring[4], lon, lat, "Polygon ring[4] (closed)");
    }
}

// =============================================================================
// Test 2: Randomized property test — multiple trials with random coordinates
// Verifies that for ANY coordinate values, the end-to-end pipeline always
// produces [longitude, latitude] in the serialized output.
// =============================================================================

TEST_F(E2ECoordinateConsistencyTest, Randomized_CoordinateOrder_Property) {
    constexpr int kNumTrials = 50;
    std::mt19937 rng{12345};  // Fixed seed for reproducibility
    std::uniform_real_distribution<double> lat_dist(-90.0, 90.0);
    std::uniform_real_distribution<double> lon_dist(-180.0, 180.0);

    for (int trial = 0; trial < kNumTrials; ++trial) {
        double lat = lat_dist(rng);
        double lon = lon_dist(rng);

        // Construct a layer name unique to this trial
        std::string layer_name = "trial_" + std::to_string(trial);

        Layer layer{
            .name = layer_name,
            .scale = SpatialScale::Urban,
            .features = {
                GeoFeature{
                    .id = "pt",
                    .geometry = Point{{lat, lon}},
                    .properties = {},
                },
                GeoFeature{
                    .id = "ls",
                    .geometry = LineString{{{lat, lon}, {lat + 1.0, lon + 1.0}}},
                    .properties = {},
                },
                GeoFeature{
                    .id = "pg",
                    .geometry = Polygon{{
                        {{lat, lon},
                         {lat, lon + 1.0},
                         {lat + 1.0, lon + 1.0},
                         {lat + 1.0, lon},
                         {lat, lon}},
                    }},
                    .properties = {},
                },
            },
        };

        // Save -> Load -> Serialize
        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value())
            << "Trial " << trial << ": save_layer failed";

        auto find_result = adapter_->find_layer(layer_name);
        ASSERT_TRUE(find_result.has_value())
            << "Trial " << trial << ": find_layer failed";

        std::string geojson_str =
            GeoJsonSerializer::serialize_layer(find_result.value());
        json geojson = json::parse(geojson_str);

        const auto& features = geojson["features"];
        ASSERT_EQ(features.size(), 3u) << "Trial " << trial;

        // Check Point
        {
            const auto& coord = features[0]["geometry"]["coordinates"];
            std::string ctx = "Trial " + std::to_string(trial) + " Point";
            assert_coordinate_order(coord, lon, lat, ctx);
        }

        // Check LineString first vertex
        {
            const auto& coords = features[1]["geometry"]["coordinates"];
            std::string ctx =
                "Trial " + std::to_string(trial) + " LineString[0]";
            assert_coordinate_order(coords[0], lon, lat, ctx);
        }

        // Check Polygon first ring first vertex
        {
            const auto& ring = features[2]["geometry"]["coordinates"][0];
            std::string ctx =
                "Trial " + std::to_string(trial) + " Polygon ring[0]";
            assert_coordinate_order(ring[0], lon, lat, ctx);
        }
    }
}

}  // namespace
}  // namespace garraiobide
