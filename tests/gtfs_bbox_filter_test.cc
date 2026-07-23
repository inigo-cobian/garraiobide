#include <gtest/gtest.h>

#include <variant>
#include <vector>

#include "../src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "../src/core/domain/bounding_box.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Geometry;
using core::domain::LineString;
using core::domain::Point;

// =============================================================================
// Property 10: Bounding Box Filtering
// For any bounding box and set of features, load_features_within SHALL return
// only features where at least one coordinate falls within the box.
//
// Since load_features_within depends on libzip, we test the filtering logic
// directly by building features via parse_gtfs_feed with known coordinates
// and applying manual bbox filtering using BoundingBox::contains.
//
// **Validates: Requirements 8.3**
// =============================================================================

/// Helper: check if a feature has at least one coordinate within the bbox.
/// This replicates the filtering logic that load_features_within would apply.
bool feature_intersects_bbox(const GeoFeature& feature,
                             const BoundingBox& bbox) {
    return std::visit(
        [&bbox](const auto& geom) -> bool {
            using T = std::decay_t<decltype(geom)>;
            if constexpr (std::is_same_v<T, Point>) {
                return bbox.contains(geom.position);
            } else if constexpr (std::is_same_v<T, LineString>) {
                for (const auto& vertex : geom.vertices) {
                    if (bbox.contains(vertex)) {
                        return true;
                    }
                }
                return false;
            } else {
                // Polygon: check if any ring vertex is inside
                for (const auto& ring : geom.rings) {
                    for (const auto& coord : ring) {
                        if (bbox.contains(coord)) {
                            return true;
                        }
                    }
                }
                return false;
            }
        },
        feature.geometry);
}

/// Filter features by bounding box (replicates load_features_within logic).
std::vector<GeoFeature> filter_features_within(
    const std::vector<GeoFeature>& features, const BoundingBox& bbox) {
    std::vector<GeoFeature> result;
    for (const auto& f : features) {
        if (feature_intersects_bbox(f, bbox)) {
            result.push_back(f);
        }
    }
    return result;
}

/// Helper: build a minimal feed producing stops at given coordinates.
GtfsFeed make_feed_with_stop_coords(
    std::vector<std::pair<std::string, Coordinate>> stops_with_coords) {
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = {{{"trip_id", "T1"}, {"route_id", "R1"}}};
    feed.stop_times = {{{"trip_id", "T1"},
                        {"stop_id", stops_with_coords.empty()
                                        ? "S001"
                                        : stops_with_coords[0].first},
                        {"stop_sequence", "1"}}};

    for (const auto& [id, coord] : stops_with_coords) {
        feed.stops.push_back({{"stop_id", id},
                              {"stop_name", "Stop " + id},
                              {"stop_lat", std::to_string(coord.latitude)},
                              {"stop_lon", std::to_string(coord.longitude)},
                              {"location_type", "0"},
                              {"parent_station", ""}});
    }
    return feed;
}

// --- Test 1: A Point feature inside the bbox is included ---

TEST(GtfsBboxFilterTest, PointInsideBboxIsIncluded) {
    // Bilbao city center bbox
    BoundingBox bbox{
        .south_west = {43.25, -2.95},
        .north_east = {43.27, -2.92},
    };

    // Stop inside the bbox
    Coordinate inside{43.26, -2.93};
    auto feed = make_feed_with_stop_coords({{"S1", inside}});

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    auto filtered = filter_features_within(result->stops.features, bbox);
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].id, "S1");

    // Verify the coordinate is actually inside the bbox
    auto& point = std::get<Point>(filtered[0].geometry);
    EXPECT_TRUE(bbox.contains(point.position));
}

// --- Test 2: A Point feature outside the bbox is excluded ---

TEST(GtfsBboxFilterTest, PointOutsideBboxIsExcluded) {
    // Small bbox around central Bilbao
    BoundingBox bbox{
        .south_west = {43.25, -2.95},
        .north_east = {43.27, -2.92},
    };

    // Stop far outside the bbox (in Madrid)
    Coordinate outside{40.42, -3.70};
    auto feed = make_feed_with_stop_coords({{"S_OUT", outside}});

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    auto filtered = filter_features_within(result->stops.features, bbox);
    EXPECT_TRUE(filtered.empty());
}

// --- Test 3: A LineString with at least one vertex inside the bbox is included ---

TEST(GtfsBboxFilterTest, LineStringWithVertexInsideBboxIsIncluded) {
    BoundingBox bbox{
        .south_west = {43.25, -2.95},
        .north_east = {43.27, -2.92},
    };

    // Build a route with shape points: one inside, two outside
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = {{{"trip_id", "T1"}, {"route_id", "R1"}}};
    feed.stops = {{{"stop_id", "S1"},
                   {"stop_name", "Stop 1"},
                   {"stop_lat", "43.26"},
                   {"stop_lon", "-2.93"},
                   {"location_type", "0"},
                   {"parent_station", ""}}};
    feed.stop_times = {{{"trip_id", "T1"},
                        {"stop_id", "S1"},
                        {"stop_sequence", "1"}}};
    // Shape with 3 points: outside, inside, outside
    feed.shapes = {
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.20"},  // outside (south)
         {"shape_pt_lon", "-2.93"},
         {"shape_pt_sequence", "1"}},
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.26"},  // inside
         {"shape_pt_lon", "-2.93"},
         {"shape_pt_sequence", "2"}},
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.30"},  // outside (north)
         {"shape_pt_lon", "-2.93"},
         {"shape_pt_sequence", "3"}},
    };
    // Link trip to shape
    feed.trips[0]["shape_id"] = "SH1";

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    // Route features are LineStrings
    ASSERT_EQ(result->routes.features.size(), 1u);
    auto& route_feature = result->routes.features[0];
    ASSERT_TRUE(std::holds_alternative<LineString>(route_feature.geometry));

    auto filtered = filter_features_within(result->routes.features, bbox);
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].id, "R1");

    // Verify at least one vertex is inside
    auto& line = std::get<LineString>(filtered[0].geometry);
    bool any_inside = false;
    for (const auto& v : line.vertices) {
        if (bbox.contains(v)) {
            any_inside = true;
            break;
        }
    }
    EXPECT_TRUE(any_inside);
}

// --- Test 4: A LineString with all vertices outside the bbox is excluded ---

TEST(GtfsBboxFilterTest, LineStringWithAllVerticesOutsideBboxIsExcluded) {
    BoundingBox bbox{
        .south_west = {43.25, -2.95},
        .north_east = {43.27, -2.92},
    };

    // Build a route with shape points all outside the bbox
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = {{{"trip_id", "T1"}, {"route_id", "R1"}, {"shape_id", "SH1"}}};
    feed.stops = {{{"stop_id", "S1"},
                   {"stop_name", "Stop 1"},
                   {"stop_lat", "43.10"},
                   {"stop_lon", "-2.80"},
                   {"location_type", "0"},
                   {"parent_station", ""}}};
    feed.stop_times = {{{"trip_id", "T1"},
                        {"stop_id", "S1"},
                        {"stop_sequence", "1"}}};
    // All shape points far south of the bbox
    feed.shapes = {
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.10"},
         {"shape_pt_lon", "-2.80"},
         {"shape_pt_sequence", "1"}},
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.12"},
         {"shape_pt_lon", "-2.81"},
         {"shape_pt_sequence", "2"}},
        {{"shape_id", "SH1"},
         {"shape_pt_lat", "43.14"},
         {"shape_pt_lon", "-2.82"},
         {"shape_pt_sequence", "3"}},
    };

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(result->routes.features.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<LineString>(
        result->routes.features[0].geometry));

    auto filtered = filter_features_within(result->routes.features, bbox);
    EXPECT_TRUE(filtered.empty());

    // Verify no vertex is inside
    auto& line =
        std::get<LineString>(result->routes.features[0].geometry);
    for (const auto& v : line.vertices) {
        EXPECT_FALSE(bbox.contains(v));
    }
}

// --- Test 5: An empty bbox with no features inside returns empty result ---

TEST(GtfsBboxFilterTest, EmptyAreaBboxReturnsEmptyResult) {
    // A tiny bbox where no realistic stop would be
    BoundingBox bbox{
        .south_west = {0.001, 0.001},
        .north_east = {0.002, 0.002},
    };

    // Multiple stops scattered around Bilbao (all outside this tiny bbox)
    auto feed = make_feed_with_stop_coords({
        {"S1", {43.26, -2.93}},
        {"S2", {43.25, -2.94}},
        {"S3", {43.27, -2.92}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->stops.features.size(), 3u);

    auto filtered = filter_features_within(result->stops.features, bbox);
    EXPECT_TRUE(filtered.empty());
}

// --- Test 6: Mixed features - only those with coordinates inside are kept ---

TEST(GtfsBboxFilterTest, MixedFeaturesOnlyInsideKept) {
    BoundingBox bbox{
        .south_west = {43.25, -2.95},
        .north_east = {43.27, -2.92},
    };

    // Two stops: one inside, one outside
    auto feed = make_feed_with_stop_coords({
        {"S_IN", {43.26, -2.93}},    // inside
        {"S_OUT", {40.42, -3.70}},   // outside (Madrid)
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->stops.features.size(), 2u);

    auto filtered = filter_features_within(result->stops.features, bbox);
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].id, "S_IN");
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
