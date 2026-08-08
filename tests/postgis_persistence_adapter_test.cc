#include <gtest/gtest.h>

#include "../src/adapters/persistence/postgis_persistence_adapter.h"
#include "mocks/mock_transit_repository.h"

namespace garraiobide::adapters::persistence {
namespace {

using core::domain::StopType;
using core::ports::PersistenceError;

class PostgisPersistenceAdapterTest : public ::testing::Test {
   protected:
    tests::MockTransitRepository mock_repo_;
    PostgisPersistenceAdapter adapter_{mock_repo_};

    void SetUp() override {
        // Seed with test data
        mock_repo_.save_agency({.id = "metro_bilbao", .name = "Metro Bilbao"});
        mock_repo_.save_route({.id = "L1", .agency_id = "metro_bilbao",
            .short_name = "L1", .route_type = 1,
            .geometry = core::domain::LineString{{{43.25, -2.95}, {43.28, -2.92}}}});
        mock_repo_.save_stop({.id = "moyua", .name = "Moyua",
            .position = {43.2630, -2.9350},
            .stop_type = StopType::ParentStation});
        mock_repo_.save_stop({.id = "indautxu", .name = "Indautxu",
            .position = {43.2600, -2.9400},
            .stop_type = StopType::Standalone});
        mock_repo_.save_entrance({.id = "e1", .stop_id = "moyua",
            .name = "North", .position = {43.2632, -2.9348}});
    }
};

TEST_F(PostgisPersistenceAdapterTest, ListLayersReturnsRouteAndStopLayers) {
    auto result = adapter_.list_layers();
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result->size(), 2);
    // Should contain something ending in _routes and _stops
    bool has_routes = false, has_stops = false;
    for (const auto& name : *result) {
        if (name.ends_with("_routes")) has_routes = true;
        if (name.ends_with("_stops")) has_stops = true;
    }
    EXPECT_TRUE(has_routes);
    EXPECT_TRUE(has_stops);
}

TEST_F(PostgisPersistenceAdapterTest, FindRoutesLayer) {
    auto result = adapter_.find_layer("metro_bilbao_routes");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "metro_bilbao_routes");
    EXPECT_EQ(result->features.size(), 1);
    EXPECT_EQ(result->features[0].id.value(), "L1");
}

TEST_F(PostgisPersistenceAdapterTest, FindStopsLayer) {
    auto result = adapter_.find_layer("metro_bilbao_stops");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "metro_bilbao_stops");
    EXPECT_EQ(result->features.size(), 2);
}

TEST_F(PostgisPersistenceAdapterTest, FindGenericRoutesLayer) {
    auto result = adapter_.find_layer("routes");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->features.size(), 1);
}

TEST_F(PostgisPersistenceAdapterTest, FindGenericStopsLayer) {
    auto result = adapter_.find_layer("stops");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->features.size(), 2);
}

TEST_F(PostgisPersistenceAdapterTest, FindEntrancesLayer) {
    auto result = adapter_.find_layer("entrances");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->features.size(), 1);
    EXPECT_EQ(result->features[0].id.value(), "e1");
}

TEST_F(PostgisPersistenceAdapterTest, FindUnknownLayerReturnsNotFound) {
    auto result = adapter_.find_layer("nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PersistenceError::NotFound);
}

TEST_F(PostgisPersistenceAdapterTest, QueryFeaturesReturnsStopsInBbox) {
    core::domain::BoundingBox bilbao{
        .south_west = {43.25, -2.96},
        .north_east = {43.28, -2.92},
    };
    auto result = adapter_.query_features(bilbao);
    ASSERT_TRUE(result.has_value());
    // Should include stops in bbox plus all routes
    EXPECT_GE(result->size(), 2);  // At least 1 stop + 1 route
}

TEST_F(PostgisPersistenceAdapterTest, QueryFeaturesEmptyBbox) {
    core::domain::BoundingBox far_away{
        .south_west = {40.0, -3.8},
        .north_east = {40.5, -3.6},
    };
    auto result = adapter_.query_features(far_away);
    ASSERT_TRUE(result.has_value());
    // No stops in bbox, but routes are still included (no spatial filter on routes)
    EXPECT_EQ(result->size(), 1);  // Just the route
}

TEST_F(PostgisPersistenceAdapterTest, SaveLayerIsNoOp) {
    core::domain::Layer layer{.name = "test", .features = {}};
    auto result = adapter_.save_layer(layer);
    EXPECT_TRUE(result.has_value());
}

TEST_F(PostgisPersistenceAdapterTest, RemoveLayerNotSupported) {
    auto result = adapter_.remove_layer("routes");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PersistenceError::WriteError);
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
