#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

#include "../src/adapters/ingestion/gtfs/gtfs_parser.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"
#include "../src/core/domain/properties.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

// =============================================================================
// Property 5: Stop Classification
// For any stop record, the stop_type property SHALL be:
//   "parent_station" if location_type == "1",
//   "child_stop" if parent_station field is non-empty,
//   "standalone" otherwise.
// Additionally, any stop with a non-empty parent_station field SHALL have a
// "parent_station" property containing that value.
// **Validates: Requirements 4.3, 4.4, 4.5, 4.6**
// =============================================================================

/// Helper: build a minimal GtfsFeed with the given stops.
/// Includes one agency, one route, one trip, and one stop_time so
/// parse_gtfs_feed succeeds.
GtfsFeed make_feed_with_stops(std::vector<CsvRow> stops) {
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}}};
    feed.routes = {{{"route_id", "R1"},
                    {"route_short_name", "1"},
                    {"route_long_name", "Route One"},
                    {"route_type", "3"}}};
    feed.trips = {{{"trip_id", "T1"}, {"route_id", "R1"}}};
    feed.stop_times = {{{"trip_id", "T1"},
                        {"stop_id", "S001"},
                        {"stop_sequence", "1"}}};
    feed.stops = std::move(stops);
    return feed;
}

// --- Test 1: location_type="1" → stop_type="parent_station" ---

TEST(GtfsStopClassificationTest, LocationTypeOne_ParentStation) {
    auto feed = make_feed_with_stops({
        {{"stop_id", "STATION1"},
         {"stop_name", "Main Station"},
         {"stop_lat", "43.26271"},
         {"stop_lon", "-2.93467"},
         {"location_type", "1"},
         {"parent_station", ""}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& features = result->stops.features;
    ASSERT_EQ(features.size(), 1u);

    auto type_it = features[0].properties.find("stop_type");
    ASSERT_NE(type_it, features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(type_it->second), "parent_station");
}

// --- Test 2: Non-empty parent_station → stop_type="child_stop" + parent_station property ---

TEST(GtfsStopClassificationTest, NonEmptyParentStation_ChildStop) {
    auto feed = make_feed_with_stops({
        {{"stop_id", "CHILD1"},
         {"stop_name", "Platform 1"},
         {"stop_lat", "43.26050"},
         {"stop_lon", "-2.92485"},
         {"location_type", "0"},
         {"parent_station", "STATION1"}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& features = result->stops.features;
    ASSERT_EQ(features.size(), 1u);

    // Verify stop_type is "child_stop"
    auto type_it = features[0].properties.find("stop_type");
    ASSERT_NE(type_it, features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(type_it->second), "child_stop");

    // Verify parent_station property is set to the parent's ID
    auto ps_it = features[0].properties.find("parent_station");
    ASSERT_NE(ps_it, features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(ps_it->second), "STATION1");
}

// --- Test 3: location_type="0" and empty parent_station → stop_type="standalone" ---

TEST(GtfsStopClassificationTest, LocationTypeZeroEmptyParent_Standalone) {
    auto feed = make_feed_with_stops({
        {{"stop_id", "STOP1"},
         {"stop_name", "Plaza Moyua"},
         {"stop_lat", "43.26271"},
         {"stop_lon", "-2.93467"},
         {"location_type", "0"},
         {"parent_station", ""}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& features = result->stops.features;
    ASSERT_EQ(features.size(), 1u);

    auto type_it = features[0].properties.find("stop_type");
    ASSERT_NE(type_it, features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(type_it->second), "standalone");
}

// --- Test 4: Blank/missing location_type and empty parent_station → "standalone" ---

TEST(GtfsStopClassificationTest, MissingLocationTypeEmptyParent_Standalone) {
    // Case A: location_type field is blank (empty string)
    auto feed_blank = make_feed_with_stops({
        {{"stop_id", "STOP2"},
         {"stop_name", "Bus Stop"},
         {"stop_lat", "43.25000"},
         {"stop_lon", "-2.94000"},
         {"location_type", ""},
         {"parent_station", ""}},
    });

    auto result_blank = parse_gtfs_feed(feed_blank);
    ASSERT_TRUE(result_blank.has_value());
    ASSERT_EQ(result_blank->stops.features.size(), 1u);

    auto type_it = result_blank->stops.features[0].properties.find("stop_type");
    ASSERT_NE(type_it, result_blank->stops.features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(type_it->second), "standalone");

    // Case B: location_type field is entirely absent from the row
    auto feed_missing = make_feed_with_stops({
        {{"stop_id", "STOP3"},
         {"stop_name", "Another Stop"},
         {"stop_lat", "43.26000"},
         {"stop_lon", "-2.93000"}},
    });

    auto result_missing = parse_gtfs_feed(feed_missing);
    ASSERT_TRUE(result_missing.has_value());
    ASSERT_EQ(result_missing->stops.features.size(), 1u);

    auto type_it2 = result_missing->stops.features[0].properties.find("stop_type");
    ASSERT_NE(type_it2, result_missing->stops.features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(type_it2->second), "standalone");
}

// --- Test 5: Multiple stops with mixed classifications in same feed ---

TEST(GtfsStopClassificationTest, MixedClassifications) {
    auto feed = make_feed_with_stops({
        {{"stop_id", "PARENT1"},
         {"stop_name", "Central Station"},
         {"stop_lat", "43.26271"},
         {"stop_lon", "-2.93467"},
         {"location_type", "1"},
         {"parent_station", ""}},
        {{"stop_id", "CHILD1"},
         {"stop_name", "Platform A"},
         {"stop_lat", "43.26275"},
         {"stop_lon", "-2.93470"},
         {"location_type", "0"},
         {"parent_station", "PARENT1"}},
        {{"stop_id", "ALONE1"},
         {"stop_name", "Street Stop"},
         {"stop_lat", "43.25000"},
         {"stop_lon", "-2.94000"},
         {"location_type", "0"},
         {"parent_station", ""}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& features = result->stops.features;
    ASSERT_EQ(features.size(), 3u);

    // Find features by ID
    const core::domain::GeoFeature* parent = nullptr;
    const core::domain::GeoFeature* child = nullptr;
    const core::domain::GeoFeature* standalone = nullptr;

    for (const auto& f : features) {
        if (f.id == "PARENT1") parent = &f;
        else if (f.id == "CHILD1") child = &f;
        else if (f.id == "ALONE1") standalone = &f;
    }

    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);
    ASSERT_NE(standalone, nullptr);

    EXPECT_EQ(std::get<std::string>(parent->properties.at("stop_type")),
              "parent_station");
    EXPECT_EQ(std::get<std::string>(child->properties.at("stop_type")),
              "child_stop");
    EXPECT_EQ(std::get<std::string>(standalone->properties.at("stop_type")),
              "standalone");
}

// --- Test 6: parent_station property value equals the GTFS parent_station field ---

TEST(GtfsStopClassificationTest, ParentStationPropertyMatchesFieldValue) {
    const std::string parent_id = "STATION_XYZ_42";

    auto feed = make_feed_with_stops({
        {{"stop_id", "CHILD_A"},
         {"stop_name", "Stop A"},
         {"stop_lat", "43.26050"},
         {"stop_lon", "-2.92485"},
         {"location_type", "0"},
         {"parent_station", parent_id}},
    });

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    const auto& features = result->stops.features;
    ASSERT_EQ(features.size(), 1u);

    auto ps_it = features[0].properties.find("parent_station");
    ASSERT_NE(ps_it, features[0].properties.end());
    EXPECT_EQ(std::get<std::string>(ps_it->second), parent_id);
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
