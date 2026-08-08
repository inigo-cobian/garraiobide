#include <gtest/gtest.h>

#include "../src/adapters/persistence/postgis_transit_repository.h"

namespace garraiobide::adapters::persistence {
namespace {

using Error = core::ports::TransitRepositoryError;

/// Integration test fixture for PostgisTransitRepository.
/// Requires a running PostGIS instance at localhost:5433 with schema applied.
/// Set POSTGIS_TEST_CONN to override the connection string.
class PostgisTransitRepositoryTest : public ::testing::Test {
   protected:
    std::unique_ptr<PostgisTransitRepository> repo_;

    void SetUp() override {
        const char* conn_env = std::getenv("POSTGIS_TEST_CONN");
        std::string conn_str = conn_env
            ? conn_env
            : "host=localhost port=5433 dbname=garraiobide user=test_user password=test_pass";

        try {
            repo_ = std::make_unique<PostgisTransitRepository>(conn_str);
        } catch (const std::exception& e) {
            GTEST_SKIP() << "PostGIS not available: " << e.what();
        }

        // Clean all tables before each test
        clean_tables();
    }

    void clean_tables() {
        pqxx::connection conn(
            "host=localhost port=5433 dbname=garraiobide user=test_user password=test_pass");
        pqxx::work txn(conn);
        txn.exec("DELETE FROM route_stops");
        txn.exec("DELETE FROM entrances");
        txn.exec("DELETE FROM stops");
        txn.exec("DELETE FROM routes");
        txn.exec("DELETE FROM agencies");
        txn.commit();
    }

    core::domain::Agency sample_agency() {
        return {.id = "metro_bilbao", .name = "Metro Bilbao",
                .url = "https://metrobilbao.eus", .timezone = "Europe/Madrid"};
    }

    core::domain::Route sample_route() {
        return {.id = "L1", .agency_id = "metro_bilbao", .short_name = "L1",
                .long_name = "Etxebarri - Plentzia", .route_type = 1,
                .color = "FF0000",
                .geometry = core::domain::LineString{{
                    {43.25, -2.95}, {43.26, -2.93}, {43.28, -2.92},
                }},
                .station_sequence = {
                    {.id = "s1", .name = "Etxebarri", .child_count = 2},
                    {.id = "s2", .name = "Moyua", .child_count = 3},
                }};
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
};

// ── Agency tests ──────────────────────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, AgencyCRUD) {
    auto agency = sample_agency();

    // Save
    ASSERT_TRUE(repo_->save_agency(agency).has_value());

    // Find
    auto found = repo_->find_agency("metro_bilbao");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Metro Bilbao");
    EXPECT_EQ(found->url.value(), "https://metrobilbao.eus");
    EXPECT_EQ(found->timezone.value(), "Europe/Madrid");

    // Duplicate
    auto dup = repo_->save_agency(agency);
    ASSERT_FALSE(dup.has_value());
    EXPECT_EQ(dup.error(), Error::DuplicateEntity);

    // List
    auto list = repo_->list_agencies();
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->size(), 1);

    // Remove
    ASSERT_TRUE(repo_->remove_agency("metro_bilbao").has_value());
    auto gone = repo_->find_agency("metro_bilbao");
    EXPECT_EQ(gone.error(), Error::NotFound);
}

// ── Route tests ───────────────────────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, RouteCRUD) {
    ASSERT_TRUE(repo_->save_agency(sample_agency()).has_value());

    auto route = sample_route();
    ASSERT_TRUE(repo_->save_route(route).has_value());

    // Find and verify geometry + station_sequence round-trip
    auto found = repo_->find_route("L1");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->short_name.value(), "L1");
    EXPECT_EQ(found->long_name.value(), "Etxebarri - Plentzia");
    EXPECT_EQ(found->route_type, 1);
    EXPECT_EQ(found->color.value(), "FF0000");

    // Geometry round-trip
    ASSERT_TRUE(found->geometry.has_value());
    ASSERT_TRUE(std::holds_alternative<core::domain::LineString>(*found->geometry));
    auto& ls = std::get<core::domain::LineString>(*found->geometry);
    EXPECT_EQ(ls.vertices.size(), 3);
    EXPECT_NEAR(ls.vertices[0].latitude, 43.25, 0.0001);

    // Station sequence round-trip
    EXPECT_EQ(found->station_sequence.size(), 2);
    EXPECT_EQ(found->station_sequence[0].id, "s1");
    EXPECT_EQ(found->station_sequence[0].name, "Etxebarri");
    EXPECT_EQ(found->station_sequence[1].child_count, 3);

    // Remove
    ASSERT_TRUE(repo_->remove_route("L1").has_value());
    EXPECT_EQ(repo_->find_route("L1").error(), Error::NotFound);
}

TEST_F(PostgisTransitRepositoryTest, RouteFK_ViolationWithoutAgency) {
    auto route = sample_route();
    auto result = repo_->save_route(route);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ForeignKeyViolation);
}

TEST_F(PostgisTransitRepositoryTest, ListRoutesFilterByAgency) {
    ASSERT_TRUE(repo_->save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_->save_agency({.id = "other", .name = "Other"}).has_value());
    ASSERT_TRUE(repo_->save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_->save_route(
        {.id = "X1", .agency_id = "other", .route_type = 3}).has_value());

    auto all = repo_->list_routes("");
    ASSERT_TRUE(all.has_value());
    EXPECT_EQ(all->size(), 2);

    auto filtered = repo_->list_routes("metro_bilbao");
    ASSERT_TRUE(filtered.has_value());
    EXPECT_EQ(filtered->size(), 1);
    EXPECT_EQ((*filtered)[0].id, "L1");
}

// ── Stop tests ────────────────────────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, StopCRUD) {
    auto stop = sample_parent_stop();
    ASSERT_TRUE(repo_->save_stop(stop).has_value());

    auto found = repo_->find_stop("moyua");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Moyua");
    EXPECT_EQ(found->code.value(), "MOY");
    EXPECT_NEAR(found->position.latitude, 43.2630, 0.0001);
    EXPECT_NEAR(found->position.longitude, -2.9350, 0.0001);
    EXPECT_EQ(found->stop_type, core::domain::StopType::ParentStation);
}

TEST_F(PostgisTransitRepositoryTest, StopParentChild) {
    ASSERT_TRUE(repo_->save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_->save_stop(sample_child_stop()).has_value());

    auto children = repo_->find_children_of("moyua");
    ASSERT_TRUE(children.has_value());
    EXPECT_EQ(children->size(), 1);
    EXPECT_EQ((*children)[0].id, "moyua_1");
    EXPECT_EQ((*children)[0].parent_stop_id.value(), "moyua");
}

TEST_F(PostgisTransitRepositoryTest, SpatialQueryStops) {
    ASSERT_TRUE(repo_->save_stop(sample_parent_stop()).has_value());

    // Bbox around Bilbao
    core::domain::BoundingBox bilbao{
        .south_west = {43.25, -2.96},
        .north_east = {43.28, -2.92},
    };
    auto result = repo_->query_stops(bilbao);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);

    // Bbox around Madrid (far away)
    core::domain::BoundingBox madrid{
        .south_west = {40.3, -3.8},
        .north_east = {40.5, -3.6},
    };
    auto empty = repo_->query_stops(madrid);
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());
}

// ── Entrance tests ────────────────────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, EntranceCRUD) {
    ASSERT_TRUE(repo_->save_stop(sample_parent_stop()).has_value());

    core::domain::Entrance entrance{
        .id = "moyua_north", .stop_id = "moyua",
        .name = "North Entrance", .position = {43.2632, -2.9348},
    };
    ASSERT_TRUE(repo_->save_entrance(entrance).has_value());

    auto found = repo_->find_entrance("moyua_north");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name.value(), "North Entrance");
    EXPECT_EQ(found->stop_id, "moyua");
    EXPECT_NEAR(found->position.latitude, 43.2632, 0.0001);

    // List
    auto list = repo_->list_entrances("moyua");
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->size(), 1);

    // Remove
    ASSERT_TRUE(repo_->remove_entrance("moyua_north").has_value());
    EXPECT_EQ(repo_->find_entrance("moyua_north").error(), Error::NotFound);
}

// ── Route-Stop relationship tests ─────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, RouteStopRelationships) {
    ASSERT_TRUE(repo_->save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_->save_route(sample_route()).has_value());
    ASSERT_TRUE(repo_->save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_->save_stop(sample_child_stop()).has_value());

    // Link stops to route
    ASSERT_TRUE(repo_->add_route_stop("L1", "moyua", 1).has_value());
    ASSERT_TRUE(repo_->add_route_stop("L1", "moyua_1", 2).has_value());

    // Find stops for route (ordered by sequence)
    auto stops = repo_->find_stops_for_route("L1");
    ASSERT_TRUE(stops.has_value());
    EXPECT_EQ(stops->size(), 2);
    EXPECT_EQ((*stops)[0].id, "moyua");
    EXPECT_EQ((*stops)[1].id, "moyua_1");

    // Find routes for stop
    auto routes = repo_->find_routes_for_stop("moyua");
    ASSERT_TRUE(routes.has_value());
    EXPECT_EQ(routes->size(), 1);
    EXPECT_EQ((*routes)[0].id, "L1");

    // Remove route-stop link
    ASSERT_TRUE(repo_->remove_route_stop("L1", "moyua").has_value());
    auto after = repo_->find_stops_for_route("L1");
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->size(), 1);
}

TEST_F(PostgisTransitRepositoryTest, RouteStopFK_Violation) {
    auto result = repo_->add_route_stop("nonexist", "nonexist", 1);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::ForeignKeyViolation);
}

// ── Cascade tests ─────────────────────────────────────────────────────────

TEST_F(PostgisTransitRepositoryTest, RemoveAgencyCascadesToRoutes) {
    ASSERT_TRUE(repo_->save_agency(sample_agency()).has_value());
    ASSERT_TRUE(repo_->save_route(sample_route()).has_value());

    ASSERT_TRUE(repo_->remove_agency("metro_bilbao").has_value());
    EXPECT_EQ(repo_->find_route("L1").error(), Error::NotFound);
}

TEST_F(PostgisTransitRepositoryTest, RemoveStopCascadesToEntrances) {
    ASSERT_TRUE(repo_->save_stop(sample_parent_stop()).has_value());
    ASSERT_TRUE(repo_->save_entrance(
        {.id = "e1", .stop_id = "moyua", .name = "Main", .position = {43.26, -2.93}})
        .has_value());

    ASSERT_TRUE(repo_->remove_stop("moyua").has_value());
    EXPECT_EQ(repo_->find_entrance("e1").error(), Error::NotFound);
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
