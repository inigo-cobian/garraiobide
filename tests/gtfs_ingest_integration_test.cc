#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <variant>

#include "src/adapters/ingestion/gtfs/csv_parser.h"
#include "src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"

namespace garraiobide::tests {
namespace {

using adapters::ingestion::gtfs::GtfsFeed;
using adapters::ingestion::gtfs::parse_csv;
using adapters::ingestion::gtfs::parse_gtfs_feed;
using adapters::persistence::FilePersistenceAdapter;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;

/// Helper to build a minimal but valid GtfsFeed from CSV strings.
GtfsFeed build_test_feed() {
    GtfsFeed feed;

    feed.agency = parse_csv(
        "agency_id,agency_name,agency_url,agency_timezone\n"
        "A1,Test Transit,http://example.com,Europe/Madrid\n");

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
        "ST1,Stop A,43.2630,-2.9350\n"
        "ST2,Stop B,43.2650,-2.9300\n"
        "ST3,Stop C,43.2700,-2.9250\n");

    feed.stop_times = parse_csv(
        "trip_id,arrival_time,departure_time,stop_id,stop_sequence\n"
        "T1,08:00:00,08:00:00,ST1,1\n"
        "T1,08:05:00,08:05:00,ST2,2\n"
        "T2,09:00:00,09:00:00,ST2,1\n"
        "T2,09:10:00,09:10:00,ST3,2\n");

    feed.shapes = {};  // No shapes — routes built from stop_times

    return feed;
}

/// Test fixture with temp directory setup/teardown.
class GtfsIngestIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("gtfs_integ_" + std::to_string(
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

// --- Test 1: Full pipeline saves two layer files with correct names ---

TEST_F(GtfsIngestIntegrationTest, FullPipelineSavesTwoLayerFiles) {
    auto feed = build_test_feed();
    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value()) << "parse_gtfs_feed failed";

    FilePersistenceAdapter adapter(temp_dir_);

    auto save_routes = adapter.save_layer(result->routes);
    ASSERT_TRUE(save_routes.has_value()) << "save routes layer failed";

    auto save_stops = adapter.save_layer(result->stops);
    ASSERT_TRUE(save_stops.has_value()) << "save stops layer failed";

    // Verify output files exist with expected names
    EXPECT_TRUE(std::filesystem::exists(temp_dir_ / "test_transit_routes.json"));
    EXPECT_TRUE(std::filesystem::exists(temp_dir_ / "test_transit_stops.json"));
}

// --- Test 2: Route layer has LineString geometry ---

TEST_F(GtfsIngestIntegrationTest, RouteLayerHasLineStringGeometry) {
    auto feed = build_test_feed();
    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes;
    ASSERT_FALSE(routes.features.empty());

    for (const auto& feature : routes.features) {
        EXPECT_TRUE(std::holds_alternative<LineString>(feature.geometry))
            << "Route feature " << feature.id.value_or("(no id)")
            << " should have LineString geometry";
    }
}

// --- Test 3: Stop layer has Point geometry ---

TEST_F(GtfsIngestIntegrationTest, StopLayerHasPointGeometry) {
    auto feed = build_test_feed();
    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& stops = result->stops;
    ASSERT_FALSE(stops.features.empty());

    for (const auto& feature : stops.features) {
        EXPECT_TRUE(std::holds_alternative<Point>(feature.geometry))
            << "Stop feature " << feature.id.value_or("(no id)")
            << " should have Point geometry";
    }
}

// --- Test 4: Layer names follow normalized naming convention ---

TEST_F(GtfsIngestIntegrationTest, LayerNamesFollowNormalizedConvention) {
    auto feed = build_test_feed();
    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    // Agency name "Test Transit" → normalized to "test_transit"
    EXPECT_EQ(result->routes.name, "test_transit_routes");
    EXPECT_EQ(result->stops.name, "test_transit_stops");
}

}  // namespace
}  // namespace garraiobide::tests
