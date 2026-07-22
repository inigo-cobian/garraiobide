#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/properties.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

// =============================================================================
// Property 6: Feature ID Assignment
// Route features have id=route_id, stop features have id=stop_id.
//
// Property 8: Feature Count
// Number of route features equals number of routes.txt records; number of
// stop features equals number of stops.txt records (with valid lat/lon).
//
// Property 9: Metadata Preservation
// route_short_name, route_long_name, route_type preserved as properties.
// Empty/missing fields are omitted rather than stored as empty strings.
//
// **Validates: Requirements 5.3, 5.4, 5.5, 6.1, 6.2**
// =============================================================================

/// Helper: build a minimal GtfsFeed suitable for metadata tests.
GtfsFeed make_minimal_feed() {
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = {{{"trip_id", "T1"}, {"route_id", "R1"}}};
    feed.stop_times = {{{"trip_id", "T1"},
                        {"stop_id", "S1"},
                        {"stop_sequence", "1"}}};
    feed.stops = {{{"stop_id", "S1"},
                   {"stop_name", "Stop One"},
                   {"stop_lat", "43.26271"},
                   {"stop_lon", "-2.93467"}}};
    return feed;
}

// ---------------------------------------------------------------------------
// Test 1: Route feature ID equals route_id from the routes data
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, RouteFeatureIdEqualsRouteId) {
    GtfsFeed feed = make_minimal_feed();
    feed.routes = {
        {{"route_id", "ROUTE_A"},
         {"route_short_name", "A"},
         {"route_long_name", "Alpha"},
         {"route_type", "3"}},
        {{"route_id", "ROUTE_B"},
         {"route_short_name", "B"},
         {"route_long_name", "Beta"},
         {"route_type", "1"}},
    };
    feed.trips = {
        {{"trip_id", "T1"}, {"route_id", "ROUTE_A"}},
        {{"trip_id", "T2"}, {"route_id", "ROUTE_B"}},
    };
    feed.stop_times = {
        {{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
        {{"trip_id", "T2"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
    };

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& route_features = result->routes.features;
    ASSERT_EQ(route_features.size(), 2u);

    EXPECT_EQ(route_features[0].id, "ROUTE_A");
    EXPECT_EQ(route_features[1].id, "ROUTE_B");
}

// ---------------------------------------------------------------------------
// Test 2: Stop feature ID equals stop_id from the stops data
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, StopFeatureIdEqualsStopId) {
    GtfsFeed feed = make_minimal_feed();
    feed.stops = {
        {{"stop_id", "STOP_X"},
         {"stop_name", "Stop X"},
         {"stop_lat", "43.26000"},
         {"stop_lon", "-2.93000"}},
        {{"stop_id", "STOP_Y"},
         {"stop_name", "Stop Y"},
         {"stop_lat", "43.27000"},
         {"stop_lon", "-2.94000"}},
    };

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& stop_features = result->stops.features;
    ASSERT_EQ(stop_features.size(), 2u);

    EXPECT_EQ(stop_features[0].id, "STOP_X");
    EXPECT_EQ(stop_features[1].id, "STOP_Y");
}

// ---------------------------------------------------------------------------
// Test 3: Number of route features equals number of route records in the feed
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, RouteFeatureCountEqualsRouteRecords) {
    GtfsFeed feed = make_minimal_feed();
    feed.routes = {
        {{"route_id", "R1"}, {"route_short_name", "1"},
         {"route_long_name", "One"}, {"route_type", "3"}},
        {{"route_id", "R2"}, {"route_short_name", "2"},
         {"route_long_name", "Two"}, {"route_type", "3"}},
        {{"route_id", "R3"}, {"route_short_name", "3"},
         {"route_long_name", "Three"}, {"route_type", "1"}},
    };
    feed.trips = {
        {{"trip_id", "T1"}, {"route_id", "R1"}},
        {{"trip_id", "T2"}, {"route_id", "R2"}},
        {{"trip_id", "T3"}, {"route_id", "R3"}},
    };
    feed.stop_times = {
        {{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
        {{"trip_id", "T2"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
        {{"trip_id", "T3"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
    };

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->routes.features.size(), feed.routes.size());
}

// ---------------------------------------------------------------------------
// Test 4: Number of stop features equals number of stop records with valid
//         lat/lon (stops missing lat/lon are skipped)
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, StopFeatureCountEqualsValidStopRecords) {
    GtfsFeed feed = make_minimal_feed();
    feed.stops = {
        {{"stop_id", "S1"},
         {"stop_name", "Valid Stop 1"},
         {"stop_lat", "43.26000"},
         {"stop_lon", "-2.93000"}},
        {{"stop_id", "S2"},
         {"stop_name", "Valid Stop 2"},
         {"stop_lat", "43.27000"},
         {"stop_lon", "-2.94000"}},
        // This stop is missing stop_lat — should be skipped
        {{"stop_id", "S3"},
         {"stop_name", "Invalid Stop"},
         {"stop_lon", "-2.95000"}},
        // This stop is missing stop_lon — should be skipped
        {{"stop_id", "S4"},
         {"stop_name", "Also Invalid"},
         {"stop_lat", "43.28000"}},
    };

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    // Only 2 stops have both stop_lat and stop_lon
    EXPECT_EQ(result->stops.features.size(), 2u);
}

// ---------------------------------------------------------------------------
// Test 5: route_short_name, route_long_name preserved as string properties
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, RouteShortAndLongNamePreserved) {
    GtfsFeed feed = make_minimal_feed();
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "L3"},
                    {"route_long_name", "Etxebarri - Kukullaga"},
                    {"route_type", "1"}}};

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& feature = result->routes.features[0];

    auto sn_it = feature.properties.find("route_short_name");
    ASSERT_NE(sn_it, feature.properties.end());
    EXPECT_EQ(std::get<std::string>(sn_it->second), "L3");

    auto ln_it = feature.properties.find("route_long_name");
    ASSERT_NE(ln_it, feature.properties.end());
    EXPECT_EQ(std::get<std::string>(ln_it->second), "Etxebarri - Kukullaga");
}

// ---------------------------------------------------------------------------
// Test 6: route_type preserved as int64_t property
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, RouteTypePreservedAsInt64) {
    GtfsFeed feed = make_minimal_feed();
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "M"},
                    {"route_long_name", "Metro"},
                    {"route_type", "1"}}};

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& feature = result->routes.features[0];

    auto rt_it = feature.properties.find("route_type");
    ASSERT_NE(rt_it, feature.properties.end());
    // Verify it's stored as int64_t, not string
    ASSERT_TRUE(std::holds_alternative<int64_t>(rt_it->second));
    EXPECT_EQ(std::get<int64_t>(rt_it->second), 1);
}

// ---------------------------------------------------------------------------
// Test 7: Route without route_color — property should be absent (not empty)
// ---------------------------------------------------------------------------

TEST(GtfsMetadataTest, RouteWithoutColorPropertyAbsent) {
    GtfsFeed feed = make_minimal_feed();
    // No route_color field at all
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "X"},
                    {"route_long_name", "Express"},
                    {"route_type", "3"}}};

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& feature = result->routes.features[0];

    // route_color should not be present
    EXPECT_EQ(feature.properties.find("route_color"), feature.properties.end());

    // Also verify that an explicitly empty route_color is omitted
    GtfsFeed feed2 = make_minimal_feed();
    feed2.routes = {{{"route_id", "R2"},
                     {"route_short_name", "Y"},
                     {"route_long_name", "Local"},
                     {"route_type", "3"},
                     {"route_color", ""}}};

    auto result2 = parse_gtfs_feed(feed2);
    ASSERT_TRUE(result2.has_value());

    const auto& feature2 = result2->routes.features[0];
    EXPECT_EQ(feature2.properties.find("route_color"), feature2.properties.end());
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
