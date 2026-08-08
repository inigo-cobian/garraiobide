#include <gtest/gtest.h>

#include "../src/adapters/ingestion/gtfs/gtfs_entity_parser.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

/// Minimal GTFS feed for testing entity parsing.
GtfsFeed make_minimal_feed() {
    GtfsFeed feed;
    feed.agency = {
        {{"agency_id", "MB"}, {"agency_name", "Metro Bilbao"},
         {"agency_url", "https://metrobilbao.eus"}, {"agency_timezone", "Europe/Madrid"}},
    };
    feed.routes = {
        {{"route_id", "L1"}, {"agency_id", "MB"}, {"route_short_name", "L1"},
         {"route_long_name", "Etxebarri - Plentzia"}, {"route_type", "1"},
         {"route_color", "FF0000"}},
        {{"route_id", "L2"}, {"agency_id", "MB"}, {"route_short_name", "L2"},
         {"route_long_name", "Basauri - Kabiezes"}, {"route_type", "1"}},
    };
    feed.trips = {
        {{"trip_id", "T1"}, {"route_id", "L1"}, {"shape_id", ""}},
        {{"trip_id", "T2"}, {"route_id", "L2"}, {"shape_id", ""}},
    };
    feed.stops = {
        {{"stop_id", "S1"}, {"stop_name", "Moyua"}, {"stop_lat", "43.263"},
         {"stop_lon", "-2.935"}, {"location_type", "1"}, {"parent_station", ""}},
        {{"stop_id", "S1a"}, {"stop_name", "Moyua P1"}, {"stop_lat", "43.2631"},
         {"stop_lon", "-2.9351"}, {"location_type", "0"}, {"parent_station", "S1"}},
        {{"stop_id", "S2"}, {"stop_name", "Indautxu"}, {"stop_lat", "43.260"},
         {"stop_lon", "-2.940"}, {"location_type", "0"}, {"parent_station", ""}},
    };
    feed.stop_times = {
        {{"trip_id", "T1"}, {"stop_id", "S1a"}, {"stop_sequence", "1"}},
        {{"trip_id", "T1"}, {"stop_id", "S2"}, {"stop_sequence", "2"}},
        {{"trip_id", "T2"}, {"stop_id", "S1a"}, {"stop_sequence", "1"}},
    };
    feed.shapes = {};
    return feed;
}

TEST(GtfsEntityParserTest, ParsesAgencies) {
    auto feed = make_minimal_feed();
    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->agencies.size(), 1);
    EXPECT_EQ(result->agencies[0].id, "MB");
    EXPECT_EQ(result->agencies[0].name, "Metro Bilbao");
    EXPECT_EQ(result->agencies[0].url.value(), "https://metrobilbao.eus");
    EXPECT_EQ(result->agencies[0].timezone.value(), "Europe/Madrid");
}

TEST(GtfsEntityParserTest, ParsesStopsWithTypes) {
    auto feed = make_minimal_feed();
    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->stops.size(), 3);

    // Find parent station
    auto it = std::find_if(result->stops.begin(), result->stops.end(),
        [](const auto& s) { return s.id == "S1"; });
    ASSERT_NE(it, result->stops.end());
    EXPECT_EQ(it->stop_type, core::domain::StopType::ParentStation);
    EXPECT_EQ(it->name, "Moyua");
    EXPECT_NEAR(it->position.latitude, 43.263, 0.001);

    // Find child stop
    auto child = std::find_if(result->stops.begin(), result->stops.end(),
        [](const auto& s) { return s.id == "S1a"; });
    ASSERT_NE(child, result->stops.end());
    EXPECT_EQ(child->stop_type, core::domain::StopType::ChildStop);
    EXPECT_EQ(child->parent_stop_id.value(), "S1");

    // Find standalone
    auto standalone = std::find_if(result->stops.begin(), result->stops.end(),
        [](const auto& s) { return s.id == "S2"; });
    ASSERT_NE(standalone, result->stops.end());
    EXPECT_EQ(standalone->stop_type, core::domain::StopType::Standalone);
}

TEST(GtfsEntityParserTest, ParsesRoutes) {
    auto feed = make_minimal_feed();
    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->routes.size(), 2);

    auto l1 = std::find_if(result->routes.begin(), result->routes.end(),
        [](const auto& r) { return r.id == "L1"; });
    ASSERT_NE(l1, result->routes.end());
    EXPECT_EQ(l1->agency_id, "MB");
    EXPECT_EQ(l1->short_name.value(), "L1");
    EXPECT_EQ(l1->long_name.value(), "Etxebarri - Plentzia");
    EXPECT_EQ(l1->route_type, 1);
    EXPECT_EQ(l1->color.value(), "FF0000");
}

TEST(GtfsEntityParserTest, PropagatesRouteIdsToStops) {
    auto feed = make_minimal_feed();
    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());

    // S1a is in trip T1 (L1) and T2 (L2) → should have both route_ids
    auto s1a = std::find_if(result->stops.begin(), result->stops.end(),
        [](const auto& s) { return s.id == "S1a"; });
    ASSERT_NE(s1a, result->stops.end());
    EXPECT_EQ(s1a->route_ids.size(), 2);

    // S1 is parent of S1a → should inherit routes via propagation
    auto s1 = std::find_if(result->stops.begin(), result->stops.end(),
        [](const auto& s) { return s.id == "S1"; });
    ASSERT_NE(s1, result->stops.end());
    EXPECT_GE(s1->route_ids.size(), 2);  // Inherited from child
}

TEST(GtfsEntityParserTest, BuildsRouteStopSequences) {
    auto feed = make_minimal_feed();
    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());

    // L1 has trip T1 with stops S1a(seq=1), S2(seq=2)
    // After resolving to parents: S1, S2
    auto it = std::find_if(result->route_stop_sequences.begin(),
                           result->route_stop_sequences.end(),
        [](const auto& pair) { return pair.first == "L1"; });
    ASSERT_NE(it, result->route_stop_sequences.end());
    EXPECT_EQ(it->second.size(), 2);
    EXPECT_EQ(it->second[0], "S1");  // S1a resolved to parent S1
    EXPECT_EQ(it->second[1], "S2");
}

TEST(GtfsEntityParserTest, AgencyIdFallback) {
    GtfsFeed feed;
    feed.agency = {{{"agency_name", "Test Agency"}, {"agency_url", "http://test.com"},
                    {"agency_timezone", "UTC"}}};
    feed.routes = {{{"route_id", "R1"}, {"route_short_name", "R1"}, {"route_type", "3"}}};
    feed.trips = {};
    feed.stops = {};
    feed.stop_times = {};
    feed.shapes = {};

    auto result = parse_gtfs_entities(feed);
    ASSERT_TRUE(result.has_value());

    // Agency should get a generated id from normalized name
    EXPECT_EQ(result->agencies[0].id, "test_agency");
    // Route should use that generated agency_id
    EXPECT_EQ(result->routes[0].agency_id, "test_agency");
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
