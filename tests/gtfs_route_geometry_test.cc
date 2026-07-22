#include <gtest/gtest.h>

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
// Property 3: Route Geometry Ordering — shape points sorted by shape_pt_sequence
// Property 4: Longest Trip Selection — trip with most shape points selected
// **Validates: Requirements 3.1, 3.2, 3.4, 5.1, 5.2**
// =============================================================================

/// Helper: build a minimal GtfsFeed with a single route and provided shapes/trips/stop_times.
GtfsFeed make_route_feed(std::vector<CsvRow> trips,
                         std::vector<CsvRow> stop_times,
                         std::vector<CsvRow> stops,
                         std::vector<CsvRow> shapes) {
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = std::move(trips);
    feed.stop_times = std::move(stop_times);
    feed.stops = std::move(stops);
    feed.shapes = std::move(shapes);
    return feed;
}

// ---------------------------------------------------------------------------
// Test 1: Shape points appear in LineString in shape_pt_sequence order
//
// Property 2: Shape-Based Geometry Ordering
// For any set of shape points with distinct shape_pt_sequence values, the
// resulting LineString vertices SHALL appear in strictly ascending
// shape_pt_sequence order.
// **Validates: Requirements 3.1**
// ---------------------------------------------------------------------------

TEST(GtfsRouteGeometryTest, ShapePointsOrderedBySequence) {
    // Provide shape points deliberately out of order to confirm sorting.
    auto feed = make_route_feed(
        // trips
        {{{"trip_id", "T1"}, {"route_id", "R1"}, {"shape_id", "SH1"}}},
        // stop_times (minimal, needed for feed validity)
        {{{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}}},
        // stops
        {{{"stop_id", "S1"}, {"stop_lat", "43.26"}, {"stop_lon", "-2.93"}}},
        // shapes — given OUT of sequence order
        {{{"shape_id", "SH1"}, {"shape_pt_lat", "43.30"}, {"shape_pt_lon", "-2.90"}, {"shape_pt_sequence", "3"}},
         {{"shape_id", "SH1"}, {"shape_pt_lat", "43.26"}, {"shape_pt_lon", "-2.93"}, {"shape_pt_sequence", "1"}},
         {{"shape_id", "SH1"}, {"shape_pt_lat", "43.28"}, {"shape_pt_lon", "-2.91"}, {"shape_pt_sequence", "2"}}}
    );

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes.features;
    ASSERT_EQ(routes.size(), 1u);

    const auto& geom = routes[0].geometry;
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(geom));
    const auto& line = std::get<core::domain::LineString>(geom);

    ASSERT_EQ(line.vertices.size(), 3u);

    // Sequence 1: lat=43.26, lon=-2.93
    EXPECT_DOUBLE_EQ(line.vertices[0].latitude, 43.26);
    EXPECT_DOUBLE_EQ(line.vertices[0].longitude, -2.93);

    // Sequence 2: lat=43.28, lon=-2.91
    EXPECT_DOUBLE_EQ(line.vertices[1].latitude, 43.28);
    EXPECT_DOUBLE_EQ(line.vertices[1].longitude, -2.91);

    // Sequence 3: lat=43.30, lon=-2.90
    EXPECT_DOUBLE_EQ(line.vertices[2].latitude, 43.30);
    EXPECT_DOUBLE_EQ(line.vertices[2].longitude, -2.90);
}

// ---------------------------------------------------------------------------
// Test 2: Route without shapes — geometry built from stop coordinates in
// stop_sequence order
//
// Property 3: Stop-Based Geometry Ordering (Fallback)
// For any route with no associated shapes, the resulting LineString vertices
// SHALL correspond to stop coordinates ordered by ascending stop_sequence.
// **Validates: Requirements 3.2**
// ---------------------------------------------------------------------------

TEST(GtfsRouteGeometryTest, NoShapes_FallbackToStopSequence) {
    // Trip with NO shape_id → must fall back to stop_times ordering
    auto feed = make_route_feed(
        // trips — no shape_id
        {{{"trip_id", "T1"}, {"route_id", "R1"}}},
        // stop_times — given out of order to confirm sorting
        {{{"trip_id", "T1"}, {"stop_id", "S3"}, {"stop_sequence", "3"}},
         {{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
         {{"trip_id", "T1"}, {"stop_id", "S2"}, {"stop_sequence", "2"}}},
        // stops
        {{{"stop_id", "S1"}, {"stop_lat", "43.26"}, {"stop_lon", "-2.93"}},
         {{"stop_id", "S2"}, {"stop_lat", "43.27"}, {"stop_lon", "-2.92"}},
         {{"stop_id", "S3"}, {"stop_lat", "43.28"}, {"stop_lon", "-2.91"}}},
        // shapes — empty
        {}
    );

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes.features;
    ASSERT_EQ(routes.size(), 1u);

    const auto& geom = routes[0].geometry;
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(geom));
    const auto& line = std::get<core::domain::LineString>(geom);

    ASSERT_EQ(line.vertices.size(), 3u);

    // Ordered by stop_sequence: S1(seq=1), S2(seq=2), S3(seq=3)
    EXPECT_DOUBLE_EQ(line.vertices[0].latitude, 43.26);
    EXPECT_DOUBLE_EQ(line.vertices[0].longitude, -2.93);

    EXPECT_DOUBLE_EQ(line.vertices[1].latitude, 43.27);
    EXPECT_DOUBLE_EQ(line.vertices[1].longitude, -2.92);

    EXPECT_DOUBLE_EQ(line.vertices[2].latitude, 43.28);
    EXPECT_DOUBLE_EQ(line.vertices[2].longitude, -2.91);
}

// ---------------------------------------------------------------------------
// Test 3: Route with multiple trips — longest trip (most shape points) selected
//
// Property 4: Longest Trip Selection
// For any route with multiple trips, the system SHALL select the trip with the
// greatest number of shape points to represent the route geometry.
// **Validates: Requirements 3.3**
// ---------------------------------------------------------------------------

TEST(GtfsRouteGeometryTest, MultipleTripsDifferentLengths_LongestSelected) {
    // T1 has 2 shape points, T2 has 4 shape points → T2 should be selected
    auto feed = make_route_feed(
        // trips
        {{{"trip_id", "T1"}, {"route_id", "R1"}, {"shape_id", "SH_SHORT"}},
         {{"trip_id", "T2"}, {"route_id", "R1"}, {"shape_id", "SH_LONG"}}},
        // stop_times (minimal)
        {{{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
         {{"trip_id", "T2"}, {"stop_id", "S1"}, {"stop_sequence", "1"}}},
        // stops
        {{{"stop_id", "S1"}, {"stop_lat", "43.26"}, {"stop_lon", "-2.93"}}},
        // shapes
        {// SH_SHORT: 2 points
         {{"shape_id", "SH_SHORT"}, {"shape_pt_lat", "43.26"}, {"shape_pt_lon", "-2.93"}, {"shape_pt_sequence", "1"}},
         {{"shape_id", "SH_SHORT"}, {"shape_pt_lat", "43.27"}, {"shape_pt_lon", "-2.92"}, {"shape_pt_sequence", "2"}},
         // SH_LONG: 4 points
         {{"shape_id", "SH_LONG"}, {"shape_pt_lat", "43.26"}, {"shape_pt_lon", "-2.93"}, {"shape_pt_sequence", "1"}},
         {{"shape_id", "SH_LONG"}, {"shape_pt_lat", "43.27"}, {"shape_pt_lon", "-2.92"}, {"shape_pt_sequence", "2"}},
         {{"shape_id", "SH_LONG"}, {"shape_pt_lat", "43.28"}, {"shape_pt_lon", "-2.91"}, {"shape_pt_sequence", "3"}},
         {{"shape_id", "SH_LONG"}, {"shape_pt_lat", "43.29"}, {"shape_pt_lon", "-2.90"}, {"shape_pt_sequence", "4"}}}
    );

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes.features;
    ASSERT_EQ(routes.size(), 1u);

    const auto& geom = routes[0].geometry;
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(geom));
    const auto& line = std::get<core::domain::LineString>(geom);

    // The longest trip (SH_LONG) has 4 points
    ASSERT_EQ(line.vertices.size(), 4u);

    // Verify it's the SH_LONG shape (last point is 43.29, -2.90)
    EXPECT_DOUBLE_EQ(line.vertices[3].latitude, 43.29);
    EXPECT_DOUBLE_EQ(line.vertices[3].longitude, -2.90);
}

// ---------------------------------------------------------------------------
// Test 4: Route with shape_id but no matching shape data — fallback to stops
//
// When a trip has a shape_id but no corresponding shape points exist in
// shapes.txt, the system should fall back to stop_times-based geometry.
// **Validates: Requirements 3.2**
// ---------------------------------------------------------------------------

TEST(GtfsRouteGeometryTest, ShapeIdButNoShapeData_FallbackToStops) {
    // Trip has shape_id "SH_GHOST" but no shapes match it
    auto feed = make_route_feed(
        // trips — shape_id present but no matching data
        {{{"trip_id", "T1"}, {"route_id", "R1"}, {"shape_id", "SH_GHOST"}}},
        // stop_times
        {{{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}},
         {{"trip_id", "T1"}, {"stop_id", "S2"}, {"stop_sequence", "2"}}},
        // stops
        {{{"stop_id", "S1"}, {"stop_lat", "43.26"}, {"stop_lon", "-2.93"}},
         {{"stop_id", "S2"}, {"stop_lat", "43.27"}, {"stop_lon", "-2.92"}}},
        // shapes — empty (no data for SH_GHOST)
        {}
    );

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes.features;
    ASSERT_EQ(routes.size(), 1u);

    const auto& geom = routes[0].geometry;
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(geom));
    const auto& line = std::get<core::domain::LineString>(geom);

    // Should fall back to stop coordinates
    ASSERT_EQ(line.vertices.size(), 2u);
    EXPECT_DOUBLE_EQ(line.vertices[0].latitude, 43.26);
    EXPECT_DOUBLE_EQ(line.vertices[0].longitude, -2.93);
    EXPECT_DOUBLE_EQ(line.vertices[1].latitude, 43.27);
    EXPECT_DOUBLE_EQ(line.vertices[1].longitude, -2.92);
}

// ---------------------------------------------------------------------------
// Test 5: Coordinate mapping — shape_pt_lat → latitude, shape_pt_lon → longitude
//
// Property 7: Coordinate Mapping
// For any GTFS coordinate pair, the resulting domain Coordinate SHALL have
// latitude equal to the lat value and longitude equal to the lon value.
// **Validates: Requirements 5.1, 5.2**
// ---------------------------------------------------------------------------

TEST(GtfsRouteGeometryTest, CoordinateMappingLatLon) {
    // Use distinctive coordinates to verify no lat/lon swap
    const double expected_lat = 48.8566;   // Paris latitude
    const double expected_lon = 2.3522;    // Paris longitude

    auto feed = make_route_feed(
        // trips
        {{{"trip_id", "T1"}, {"route_id", "R1"}, {"shape_id", "SH1"}}},
        // stop_times
        {{{"trip_id", "T1"}, {"stop_id", "S1"}, {"stop_sequence", "1"}}},
        // stops
        {{{"stop_id", "S1"}, {"stop_lat", "48.8566"}, {"stop_lon", "2.3522"}}},
        // shapes — single point for simplicity
        {{{"shape_id", "SH1"}, {"shape_pt_lat", "48.8566"}, {"shape_pt_lon", "2.3522"}, {"shape_pt_sequence", "1"}},
         {{"shape_id", "SH1"}, {"shape_pt_lat", "48.8606"}, {"shape_pt_lon", "2.3376"}, {"shape_pt_sequence", "2"}}}
    );

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& routes = result->routes.features;
    ASSERT_EQ(routes.size(), 1u);

    const auto& geom = routes[0].geometry;
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(geom));
    const auto& line = std::get<core::domain::LineString>(geom);

    ASSERT_EQ(line.vertices.size(), 2u);

    // Verify shape_pt_lat → latitude (not longitude!)
    EXPECT_DOUBLE_EQ(line.vertices[0].latitude, expected_lat);
    // Verify shape_pt_lon → longitude (not latitude!)
    EXPECT_DOUBLE_EQ(line.vertices[0].longitude, expected_lon);

    // Second point
    EXPECT_DOUBLE_EQ(line.vertices[1].latitude, 48.8606);
    EXPECT_DOUBLE_EQ(line.vertices[1].longitude, 2.3376);
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
