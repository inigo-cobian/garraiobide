#include <gtest/gtest.h>

#include "mocks/mock_transit_repository.h"

namespace garraiobide::tests {
namespace {

using Error = core::ports::TransitRepositoryError;

class TransitRepositoryTest : public ::testing::Test {
   protected:
    MockTransitRepository repo_;

    core::domain::Agency sample_agency() {
        return {.id = "metro_bilbao", .name = "Metro Bilbao",
                .url = "https://metrobilbao.eus", .timezone = "Europe/Madrid"};
    }

    core::domain::Route sample_route() {
        return {.id = "L1", .agency_id = "metro_bilbao", .short_name = "L1",
                .long_name = "Etxebarri - Plentzia", .route_type = 1,
                .color = "FF0000"};
    }

    core::domain::Stop sample_parent_stop() {
        return {.id = "moyua", .name = "Moyua", .code = "MOY",
                .position = {43.2630, -2.9350},
                .stop_type = core::domain::StopType::ParentStation};
    }

    core::domain::Stop sample_child_stop() {
        return {.id = "moyua_1", .name = "Moyua - Platform 1",
                .position = {43.2631, -2.9351},
                .stop_type = core::domain::StopType::ChildStop,
                .parent_stop_id = "moyua"};
    }

    core::domain::Entrance sample_entrance() {
        return {.id = "moyua_north", .stop_id = "moyua",
                .name = "North Entrance", .position = {43.2632, -2.9348}};
    }
};

// ── Agency CRUD ───────────────────────────────────────────────────────────

TEST_F(TransitRepositoryTest, SaveAndFindAgency) {
    auto agency = sample_agency();
    ASSERT_TRUE(repo_.save_agency(agency).has_value());

    auto found = repo_.find_agency("metro_bilbao");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Metro Bilbao");
    EXPECT_EQ(found->url.value(), "https://metrobilbao.eus");
}

TEST_F(TransitRepositoryTest, DuplicateAgencyFails) {
    auto agency = sample_agency();
    ASSERT_TRUE(repo_.save_agency(agency).has_value());

    auto result = repo_.save_agency(agency);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::DuplicateEntity);
}

TEST_F(TransitRepositoryTest, FindNonexistentAgency) {
    auto result = repo_.find_agency("nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::NotFound);
}

TEST_F(TransitRepositoryTest, ListAgencies) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());

    auto list = repo_.list_agencies();
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->size(), 1);
    EXPECT_EQ((*list)[0].id, "metro_bilbao");
}

TEST_F(TransitRepositoryTest, RemoveAgency) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.remove_agency("metro_bilbao").has_value());

    auto result = repo_.find_agency("metro_bilbao");
    EXPECT_EQ(result.error(), Error::NotFound);
}

// ── Route CRUD ────────────────────────────────────────────────────────────

TEST_F(TransitRepositoryTest, SaveRouteRequiresAgency) {
    auto route = sample_route();
    auto result = repo_.save_route(route);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ForeignKeyViolation);
}

TEST_F(TransitRepositoryTest, SaveAndFindRoute) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());

    auto route = sample_route();
    ASSERT_TRUE(repo_.save_route(route).has_value());

    auto found = repo_.find_route("L1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->short_name.value(), "L1");
    EXPECT_EQ(found->agency_id, "metro_bilbao");
}

TEST_F(TransitRepositoryTest, ListRoutesFilteredByAgency) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_agency({.id = "other", .name = "Other"}).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.save_route({.id = "X1", .agency_id = "other", .route_type = 3}).has_value());

    auto all = repo_.list_routes("");
    ASSERT_TRUE(all.has_value());
    EXPECT_EQ(all->size(), 2);

    auto filtered = repo_.list_routes("metro_bilbao");
    ASSERT_TRUE(filtered.has_value());
    EXPECT_EQ(filtered->size(), 1);
    EXPECT_EQ((*filtered)[0].id, "L1");
}

TEST_F(TransitRepositoryTest, RemoveAgencyCascadesToRoutes) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.remove_agency("metro_bilbao").has_value());

    auto result = repo_.find_route("L1");
    EXPECT_EQ(result.error(), Error::NotFound);
}

// ── Stop CRUD ─────────────────────────────────────────────────────────────

TEST_F(TransitRepositoryTest, SaveAndFindStop) {
    auto stop = sample_parent_stop();
    ASSERT_TRUE(repo_.save_stop(stop).has_value());

    auto found = repo_.find_stop("moyua");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Moyua");
    EXPECT_EQ(found->stop_type, core::domain::StopType::ParentStation);
}

TEST_F(TransitRepositoryTest, FindChildrenOf) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_child_stop()).has_value());

    auto children = repo_.find_children_of("moyua");
    ASSERT_TRUE(children.has_value());
    EXPECT_EQ(children->size(), 1);
    EXPECT_EQ((*children)[0].id, "moyua_1");
}

TEST_F(TransitRepositoryTest, QueryStopsSpatial) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());

    // Bounding box around Bilbao center
    core::domain::BoundingBox bilbao{
        .south_west = {43.25, -2.96},
        .north_east = {43.28, -2.92},
    };
    auto result = repo_.query_stops(bilbao);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);

    // Bounding box far away
    core::domain::BoundingBox madrid{
        .south_west = {40.3, -3.8},
        .north_east = {40.5, -3.6},
    };
    auto empty = repo_.query_stops(madrid);
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());
}

TEST_F(TransitRepositoryTest, ListStopsFilteredByType) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_child_stop()).has_value());

    auto parents = repo_.list_stops("parent_station");
    ASSERT_TRUE(parents.has_value());
    EXPECT_EQ(parents->size(), 1);

    auto children = repo_.list_stops("child_stop");
    ASSERT_TRUE(children.has_value());
    EXPECT_EQ(children->size(), 1);

    auto all = repo_.list_stops("");
    ASSERT_TRUE(all.has_value());
    EXPECT_EQ(all->size(), 2);
}

// ── Entrance CRUD ─────────────────────────────────────────────────────────

TEST_F(TransitRepositoryTest, SaveEntranceRequiresStop) {
    auto entrance = sample_entrance();
    auto result = repo_.save_entrance(entrance);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ForeignKeyViolation);
}

TEST_F(TransitRepositoryTest, SaveAndFindEntrance) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());

    auto entrance = sample_entrance();
    ASSERT_TRUE(repo_.save_entrance(entrance).has_value());

    auto found = repo_.find_entrance("moyua_north");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name.value(), "North Entrance");
    EXPECT_EQ(found->stop_id, "moyua");
}

TEST_F(TransitRepositoryTest, ListEntrancesForStop) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.save_entrance(sample_entrance()).has_value());
    ASSERT_TRUE(repo_.save_entrance(
        {.id = "moyua_south", .stop_id = "moyua", .name = "South", .position = {43.262, -2.936}})
        .has_value());

    auto entrances = repo_.list_entrances("moyua");
    ASSERT_TRUE(entrances.has_value());
    EXPECT_EQ(entrances->size(), 2);
}

TEST_F(TransitRepositoryTest, RemoveStopCascadesToEntrances) {
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.save_entrance(sample_entrance()).has_value());
    ASSERT_TRUE(repo_.remove_stop("moyua").has_value());

    auto result = repo_.find_entrance("moyua_north");
    EXPECT_EQ(result.error(), Error::NotFound);
}

// ── Route-Stop relationships ──────────────────────────────────────────────

TEST_F(TransitRepositoryTest, AddRouteStop) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_child_stop()).has_value());

    ASSERT_TRUE(repo_.add_route_stop("L1", "moyua", 1).has_value());
    ASSERT_TRUE(repo_.add_route_stop("L1", "moyua_1", 2).has_value());

    auto stops = repo_.find_stops_for_route("L1");
    ASSERT_TRUE(stops.has_value());
    EXPECT_EQ(stops->size(), 2);
    // Should be ordered by sequence
    EXPECT_EQ((*stops)[0].id, "moyua");
    EXPECT_EQ((*stops)[1].id, "moyua_1");
}

TEST_F(TransitRepositoryTest, FindRoutesForStop) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.add_route_stop("L1", "moyua", 1).has_value());

    auto routes = repo_.find_routes_for_stop("moyua");
    ASSERT_TRUE(routes.has_value());
    EXPECT_EQ(routes->size(), 1);
    EXPECT_EQ((*routes)[0].id, "L1");
}

TEST_F(TransitRepositoryTest, AddRouteStopRequiresExistingEntities) {
    auto result = repo_.add_route_stop("nonexistent", "nonexistent", 1);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ForeignKeyViolation);
}

TEST_F(TransitRepositoryTest, RemoveRouteStop) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.add_route_stop("L1", "moyua", 1).has_value());

    ASSERT_TRUE(repo_.remove_route_stop("L1", "moyua").has_value());

    auto stops = repo_.find_stops_for_route("L1");
    ASSERT_TRUE(stops.has_value());
    EXPECT_TRUE(stops->empty());
}

TEST_F(TransitRepositoryTest, RemoveRouteCascadesToRouteStops) {
    ASSERT_TRUE(repo_.save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_.save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_.save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_.add_route_stop("L1", "moyua", 1).has_value());
    ASSERT_TRUE(repo_.remove_route("L1").has_value());

    // Route-stop link should be gone; find_routes_for_stop should return empty
    auto routes = repo_.find_routes_for_stop("moyua");
    ASSERT_TRUE(routes.has_value());
    EXPECT_TRUE(routes->empty());
}

}  // namespace
}  // namespace garraiobide::tests
