#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

#include "src/adapters/ingestion/gtfs/csv_parser.h"
#include "src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"

namespace garraiobide::tests {
namespace {

using adapters::ingestion::gtfs::GtfsFeed;
using adapters::ingestion::gtfs::parse_csv;
using adapters::ingestion::gtfs::parse_gtfs_feed;
using adapters::persistence::FilePersistenceAdapter;
using core::domain::GeoFeature;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;

/// Build a minimal feed for route round-trip testing.
GtfsFeed build_route_feed() {
    GtfsFeed feed;

    feed.agency = parse_csv(
        "agency_id,agency_name,agency_url,agency_timezone\n"
        "A1,Roundtrip Agency,http://example.com,Europe/Madrid\n");

    feed.routes = parse_csv(
        "route_id,agency_id,route_short_name,route_long_name,route_type\n"
        "R1,A1,L1,Line One,3\n"
        "R2,A1,L2,Line Two,1\n");

    feed.trips = parse_csv(
        "route_id,service_id,trip_id\n"
        "R1,S1,T1\n"
        "R2,S1,T2\n");

    feed.stops = parse_csv(
        "stop_id,stop_name,stop_lat,stop_lon\n"
        "ST1,Alpha,43.2630,-2.9350\n"
        "ST2,Beta,43.2650,-2.9300\n"
        "ST3,Gamma,43.2700,-2.9250\n");

    feed.stop_times = parse_csv(
        "trip_id,arrival_time,departure_time,stop_id,stop_sequence\n"
        "T1,08:00:00,08:00:00,ST1,1\n"
        "T1,08:05:00,08:05:00,ST2,2\n"
        "T2,09:00:00,09:00:00,ST2,1\n"
        "T2,09:10:00,09:10:00,ST3,2\n");

    feed.shapes = {};

    return feed;
}

/// Build a minimal feed for stop round-trip testing.
GtfsFeed build_stop_feed() {
    GtfsFeed feed;

    feed.agency = parse_csv(
        "agency_id,agency_name,agency_url,agency_timezone\n"
        "A1,Stop Agency,http://example.com,Europe/Madrid\n");

    feed.routes = {};
    feed.trips = {};

    feed.stops = parse_csv(
        "stop_id,stop_name,stop_lat,stop_lon\n"
        "S1,Plaza Moyua,43.2630,-2.9350\n"
        "S2,Abando,43.2600,-2.9400\n"
        "S3,Casco Viejo,43.2580,-2.9230\n");

    feed.stop_times = {};
    feed.shapes = {};

    return feed;
}

/// Test fixture with temp directory lifecycle.
class GtfsPersistenceRoundtripTest : public ::testing::Test {
   protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("gtfs_rt_" + std::to_string(
                         std::hash<std::string>{}(
                             ::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name())));
        std::filesystem::remove_all(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
};

// --- Test 1: Route layer round-trip preserves feature count and IDs ---

TEST_F(GtfsPersistenceRoundtripTest, RouteLayerRoundtripPreservesFeatures) {
    auto feed = build_route_feed();
    auto parsed = parse_gtfs_feed(feed);
    ASSERT_TRUE(parsed.has_value());

    const auto& original = parsed->routes;

    FilePersistenceAdapter adapter(temp_dir_);
    auto save_result = adapter.save_layer(original);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter.find_layer(original.name);
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    const auto& loaded = find_result.value();

    // Feature count matches
    EXPECT_EQ(loaded.features.size(), original.features.size());

    // Feature IDs match (order preserved)
    for (std::size_t i = 0; i < original.features.size(); ++i) {
        EXPECT_EQ(loaded.features[i].id, original.features[i].id)
            << "Feature ID mismatch at index " << i;
    }
}

// --- Test 2: Stop layer round-trip preserves features, IDs, and coordinates ---

TEST_F(GtfsPersistenceRoundtripTest, StopLayerRoundtripPreservesCoordinates) {
    auto feed = build_stop_feed();
    auto parsed = parse_gtfs_feed(feed);
    ASSERT_TRUE(parsed.has_value());

    const auto& original = parsed->stops;

    FilePersistenceAdapter adapter(temp_dir_);
    auto save_result = adapter.save_layer(original);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter.find_layer(original.name);
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    const auto& loaded = find_result.value();

    // Feature count matches
    ASSERT_EQ(loaded.features.size(), original.features.size());

    for (std::size_t i = 0; i < original.features.size(); ++i) {
        // ID matches
        EXPECT_EQ(loaded.features[i].id, original.features[i].id);

        // Geometry is Point with same coordinates
        ASSERT_TRUE(std::holds_alternative<Point>(loaded.features[i].geometry));
        ASSERT_TRUE(std::holds_alternative<Point>(original.features[i].geometry));

        const auto& loaded_pt = std::get<Point>(loaded.features[i].geometry);
        const auto& orig_pt = std::get<Point>(original.features[i].geometry);

        EXPECT_DOUBLE_EQ(loaded_pt.position.latitude, orig_pt.position.latitude)
            << "Latitude mismatch for feature " << i;
        EXPECT_DOUBLE_EQ(loaded_pt.position.longitude, orig_pt.position.longitude)
            << "Longitude mismatch for feature " << i;
    }
}

// --- Test 3: route_type property round-trips as int64_t ---

TEST_F(GtfsPersistenceRoundtripTest, RouteTypeRoundtripsAsInt64) {
    auto feed = build_route_feed();
    auto parsed = parse_gtfs_feed(feed);
    ASSERT_TRUE(parsed.has_value());

    const auto& original = parsed->routes;

    FilePersistenceAdapter adapter(temp_dir_);
    auto save_result = adapter.save_layer(original);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter.find_layer(original.name);
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    const auto& loaded = find_result.value();
    ASSERT_FALSE(loaded.features.empty());

    for (const auto& feature : loaded.features) {
        auto it = feature.properties.find("route_type");
        ASSERT_NE(it, feature.properties.end())
            << "route_type missing for feature " << feature.id.value_or("(no id)");

        // Verify it's stored as int64_t, not string
        EXPECT_TRUE(std::holds_alternative<int64_t>(it->second))
            << "route_type should be int64_t for feature "
            << feature.id.value_or("(no id)");

        // Verify value is correct
        if (feature.id.has_value() && feature.id.value() == "R1") {
            EXPECT_EQ(std::get<int64_t>(it->second), 3);
        } else if (feature.id.has_value() && feature.id.value() == "R2") {
            EXPECT_EQ(std::get<int64_t>(it->second), 1);
        }
    }
}

}  // namespace
}  // namespace garraiobide::tests
