#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "../src/core/domain/agency.h"
#include "../src/core/domain/entity_conversion.h"
#include "../src/core/domain/entrance.h"
#include "../src/core/domain/route.h"
#include "../src/core/domain/stop.h"

namespace garraiobide::core::domain {
namespace {

// --- Agency construction tests ---

TEST(AgencyTest, Construction) {
    Agency agency{
        .id = "agency_1",
        .name = "Metro Bilbao",
        .url = "https://www.metrobilbao.eus",
        .timezone = "Europe/Madrid",
        .lang = "eu",
        .phone = "+34 944 254 025",
    };

    EXPECT_EQ(agency.id, "agency_1");
    EXPECT_EQ(agency.name, "Metro Bilbao");
    EXPECT_EQ(agency.url.value(), "https://www.metrobilbao.eus");
    EXPECT_EQ(agency.timezone.value(), "Europe/Madrid");
    EXPECT_EQ(agency.lang.value(), "eu");
    EXPECT_EQ(agency.phone.value(), "+34 944 254 025");
}

TEST(AgencyTest, OptionalFieldsEmpty) {
    Agency agency{.id = "a1", .name = "Test"};

    EXPECT_FALSE(agency.url.has_value());
    EXPECT_FALSE(agency.timezone.has_value());
    EXPECT_FALSE(agency.lang.has_value());
    EXPECT_FALSE(agency.phone.has_value());
}

// --- Stop construction and conversion tests ---

TEST(StopTest, Construction) {
    Stop stop{
        .id = "stop_1",
        .name = "Moyua",
        .code = "MOY",
        .url = "https://example.com/moyua",
        .position = {43.2630, -2.9350},
        .stop_type = StopType::ParentStation,
        .parent_stop_id = std::nullopt,
        .route_ids = {"L1", "L2"},
    };

    EXPECT_EQ(stop.id, "stop_1");
    EXPECT_EQ(stop.name, "Moyua");
    EXPECT_EQ(stop.code.value(), "MOY");
    EXPECT_EQ(stop.stop_type, StopType::ParentStation);
    EXPECT_FALSE(stop.parent_stop_id.has_value());
    EXPECT_EQ(stop.route_ids.size(), 2);
}

TEST(StopTest, ChildStopWithParent) {
    Stop child{
        .id = "stop_1a",
        .name = "Moyua - Platform 1",
        .position = {43.2631, -2.9351},
        .stop_type = StopType::ChildStop,
        .parent_stop_id = "stop_1",
        .route_ids = {"L1"},
    };

    EXPECT_EQ(child.stop_type, StopType::ChildStop);
    EXPECT_EQ(child.parent_stop_id.value(), "stop_1");
}

TEST(StopConversionTest, StopToFeature) {
    Stop stop{
        .id = "stop_1",
        .name = "Moyua",
        .code = "MOY",
        .url = "https://example.com/moyua",
        .position = {43.2630, -2.9350},
        .stop_type = StopType::ParentStation,
        .parent_stop_id = std::nullopt,
        .route_ids = {"L1", "L2"},
    };

    auto feature = stop_to_feature(stop);

    EXPECT_EQ(feature.id.value(), "stop_1");

    // Check geometry is a Point
    ASSERT_TRUE(std::holds_alternative<Point>(feature.geometry));
    auto& point = std::get<Point>(feature.geometry);
    EXPECT_DOUBLE_EQ(point.position.latitude, 43.2630);
    EXPECT_DOUBLE_EQ(point.position.longitude, -2.9350);

    // Check properties
    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_type")), "parent_station");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_name")), "Moyua");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_code")), "MOY");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_url")),
              "https://example.com/moyua");

    // route_ids should be a JSON array string
    auto route_ids_str = std::get<std::string>(feature.properties.at("route_ids"));
    auto route_ids_json = nlohmann::json::parse(route_ids_str);
    EXPECT_TRUE(route_ids_json.is_array());
    EXPECT_EQ(route_ids_json.size(), 2);
}

TEST(StopConversionTest, ChildStopHasParentProperty) {
    Stop child{
        .id = "stop_1a",
        .name = "Platform 1",
        .position = {43.26, -2.93},
        .stop_type = StopType::ChildStop,
        .parent_stop_id = "stop_1",
    };

    auto feature = stop_to_feature(child);

    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_type")), "child_stop");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("parent_station")), "stop_1");
}

// --- Route construction and conversion tests ---

TEST(RouteTest, Construction) {
    Route route{
        .id = "L1",
        .agency_id = "metro_bilbao",
        .short_name = "L1",
        .long_name = "Etxebarri - Plentzia",
        .route_type = 1,
        .color = "FF0000",
        .text_color = "FFFFFF",
        .geometry = LineString{{
            {43.25, -2.95},
            {43.26, -2.93},
            {43.28, -2.92},
        }},
        .station_sequence = {
            {.id = "s1", .name = "Etxebarri", .child_count = 2},
            {.id = "s2", .name = "Basarrate", .child_count = 2},
        },
    };

    EXPECT_EQ(route.id, "L1");
    EXPECT_EQ(route.agency_id, "metro_bilbao");
    EXPECT_EQ(route.route_type, 1);
    EXPECT_TRUE(route.geometry.has_value());
    EXPECT_EQ(route.station_sequence.size(), 2);
}

TEST(RouteConversionTest, RouteToFeature) {
    Route route{
        .id = "L1",
        .agency_id = "metro_bilbao",
        .short_name = "L1",
        .long_name = "Etxebarri - Plentzia",
        .route_type = 1,
        .color = "FF0000",
        .text_color = "FFFFFF",
        .geometry = LineString{{
            {43.25, -2.95},
            {43.28, -2.92},
        }},
        .station_sequence = {
            {.id = "s1", .name = "Etxebarri", .child_count = 2},
        },
    };

    auto feature = route_to_feature(route);

    EXPECT_EQ(feature.id.value(), "L1");

    // Check geometry is a LineString
    ASSERT_TRUE(std::holds_alternative<LineString>(feature.geometry));
    auto& line = std::get<LineString>(feature.geometry);
    EXPECT_EQ(line.vertices.size(), 2);

    // Check properties
    EXPECT_EQ(std::get<std::string>(feature.properties.at("route_short_name")), "L1");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("route_long_name")),
              "Etxebarri - Plentzia");
    EXPECT_EQ(std::get<int64_t>(feature.properties.at("route_type")), 1);
    EXPECT_EQ(std::get<std::string>(feature.properties.at("route_color")), "FF0000");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("route_text_color")), "FFFFFF");

    // station_sequence should be a JSON array string
    auto seq_str = std::get<std::string>(feature.properties.at("station_sequence"));
    auto seq_json = nlohmann::json::parse(seq_str);
    EXPECT_TRUE(seq_json.is_array());
    EXPECT_EQ(seq_json.size(), 1);
    EXPECT_EQ(seq_json[0]["id"], "s1");
    EXPECT_EQ(seq_json[0]["name"], "Etxebarri");
    EXPECT_EQ(seq_json[0]["child_count"], 2);
}

TEST(RouteConversionTest, RouteWithoutGeometry) {
    Route route{
        .id = "R1",
        .agency_id = "a1",
        .route_type = 3,
    };

    auto feature = route_to_feature(route);

    // Should default to an empty LineString
    ASSERT_TRUE(std::holds_alternative<LineString>(feature.geometry));
    auto& line = std::get<LineString>(feature.geometry);
    EXPECT_TRUE(line.vertices.empty());
}

// --- Entrance construction and conversion tests ---

TEST(EntranceTest, Construction) {
    Entrance entrance{
        .id = "ent_1",
        .stop_id = "stop_1",
        .name = "North Entrance",
        .position = {43.2632, -2.9348},
    };

    EXPECT_EQ(entrance.id, "ent_1");
    EXPECT_EQ(entrance.stop_id, "stop_1");
    EXPECT_EQ(entrance.name.value(), "North Entrance");
}

TEST(EntranceConversionTest, EntranceToFeature) {
    Entrance entrance{
        .id = "ent_1",
        .stop_id = "stop_1",
        .name = "North Entrance",
        .position = {43.2632, -2.9348},
    };

    auto feature = entrance_to_feature(entrance);

    EXPECT_EQ(feature.id.value(), "ent_1");
    ASSERT_TRUE(std::holds_alternative<Point>(feature.geometry));
    EXPECT_EQ(std::get<std::string>(feature.properties.at("stop_id")), "stop_1");
    EXPECT_EQ(std::get<std::string>(feature.properties.at("entrance_name")), "North Entrance");
}

// --- Layer building tests ---

TEST(LayerBuildingTest, StopsToLayer) {
    std::vector<Stop> stops = {
        {.id = "s1", .name = "A", .position = {43.0, -2.9}},
        {.id = "s2", .name = "B", .position = {43.1, -2.8}},
    };

    auto layer = stops_to_layer(stops, "test_stops");

    EXPECT_EQ(layer.name, "test_stops");
    EXPECT_EQ(layer.scale, SpatialScale::Urban);
    EXPECT_EQ(layer.features.size(), 2);
    EXPECT_EQ(layer.features[0].id.value(), "s1");
    EXPECT_EQ(layer.features[1].id.value(), "s2");
}

TEST(LayerBuildingTest, RoutesToLayer) {
    std::vector<Route> routes = {
        {.id = "r1", .agency_id = "a1", .short_name = "R1", .route_type = 3,
         .geometry = LineString{{{43.0, -2.9}, {43.1, -2.8}}}},
    };

    auto layer = routes_to_layer(routes, "test_routes");

    EXPECT_EQ(layer.name, "test_routes");
    EXPECT_EQ(layer.features.size(), 1);
    EXPECT_EQ(layer.features[0].id.value(), "r1");
}

TEST(LayerBuildingTest, EntrancesToLayer) {
    std::vector<Entrance> entrances = {
        {.id = "e1", .stop_id = "s1", .name = "Main", .position = {43.0, -2.9}},
        {.id = "e2", .stop_id = "s1", .position = {43.0, -2.91}},
    };

    auto layer = entrances_to_layer(entrances, "test_entrances");

    EXPECT_EQ(layer.name, "test_entrances");
    EXPECT_EQ(layer.features.size(), 2);
}

// --- Round-trip consistency test ---

TEST(RoundTripTest, StopConversionPreservesGeometry) {
    Stop stop{
        .id = "rt_stop",
        .name = "Round Trip",
        .position = {43.2630, -2.9350},
        .stop_type = StopType::Standalone,
    };

    auto feature = stop_to_feature(stop);
    auto& point = std::get<Point>(feature.geometry);

    EXPECT_DOUBLE_EQ(point.position.latitude, stop.position.latitude);
    EXPECT_DOUBLE_EQ(point.position.longitude, stop.position.longitude);
}

TEST(RoundTripTest, RouteConversionPreservesAllVertices) {
    Route route{
        .id = "rt_route",
        .agency_id = "a1",
        .route_type = 1,
        .geometry = MultiLineString{{
            {{43.0, -2.9}, {43.1, -2.8}},
            {{43.2, -2.7}, {43.3, -2.6}},
        }},
    };

    auto feature = route_to_feature(route);
    ASSERT_TRUE(std::holds_alternative<MultiLineString>(feature.geometry));
    auto& mls = std::get<MultiLineString>(feature.geometry);
    EXPECT_EQ(mls.lines.size(), 2);
    EXPECT_EQ(mls.lines[0].size(), 2);
    EXPECT_EQ(mls.lines[1].size(), 2);
}

}  // namespace
}  // namespace garraiobide::core::domain
