#include <gtest/gtest.h>

#include "../src/adapters/ui/mock_presentation_adapter.h"
#include "../src/app/layer_service.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"
#include "mocks/mock_ingestion_adapter.h"
#include "mocks/mock_persistence_adapter.h"

namespace garraiobide::app {
namespace {

using namespace garraiobide::core::domain;
using namespace garraiobide::adapters;

class LayerServiceTest : public ::testing::Test {
   protected:
    void SetUp() override {
        service_ = std::make_unique<LayerService>(ingestion_, persistence_,
                                                   presentation_);
    }

    ingestion::MockIngestionAdapter ingestion_;
    persistence::MockPersistenceAdapter persistence_;
    ui::MockPresentationAdapter presentation_;
    std::unique_ptr<LayerService> service_;

    // Helper: create a sample feature set around Bilbao.
    static std::vector<GeoFeature> bilbao_features() {
        return {
            GeoFeature{
                .id = "plaza_moyua",
                .geometry = Point{{43.2630, -2.9350}},
                .properties = {{"name", std::string{"Plaza Moyua"}}},
            },
            GeoFeature{
                .id = "guggenheim",
                .geometry = Point{{43.2687, -2.9340}},
                .properties = {{"name", std::string{"Guggenheim Museum"}}},
            },
        };
    }
};

// --- import_layer tests ---

TEST_F(LayerServiceTest, ImportLayerSuccess) {
    ingestion_.set_features(bilbao_features());

    auto result =
        service_->import_layer("bilbao_poi", "bilbao.geojson", SpatialScale::Urban);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(presentation_.rendered_layers().size(), 1);
    EXPECT_EQ(presentation_.rendered_layers()[0].name, "bilbao_poi");
    EXPECT_EQ(presentation_.rendered_layers()[0].features.size(), 2);
    EXPECT_EQ(presentation_.messages().size(), 1);
    EXPECT_EQ(persistence_.layer_count(), 1);
}

TEST_F(LayerServiceTest, ImportLayerIngestionError) {
    ingestion_.set_error(core::ports::IngestionError::ParseError);

    auto result =
        service_->import_layer("bad", "broken.geojson", SpatialScale::Urban);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::IngestionFailed);
    EXPECT_EQ(presentation_.errors().size(), 1);
    EXPECT_EQ(persistence_.layer_count(), 0);
}

TEST_F(LayerServiceTest, ImportLayerDuplicateError) {
    ingestion_.set_features(bilbao_features());

    auto first =
        service_->import_layer("bilbao", "bilbao.geojson", SpatialScale::Urban);
    ASSERT_TRUE(first.has_value());

    // Second import with same name should fail.
    auto second =
        service_->import_layer("bilbao", "bilbao.geojson", SpatialScale::Urban);
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), LayerServiceError::DuplicateLayer);
}

// --- list_layers tests ---

TEST_F(LayerServiceTest, ListLayersEmpty) {
    auto result = service_->list_layers();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
    EXPECT_EQ(presentation_.presented_lists().size(), 1);
}

TEST_F(LayerServiceTest, ListLayersAfterImport) {
    ingestion_.set_features(bilbao_features());
    [[maybe_unused]] auto _ =
        service_->import_layer("layer_a", "a.geojson", SpatialScale::Urban);
    presentation_.clear();

    auto result = service_->list_layers();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ((*result)[0], "layer_a");
}

// --- show_layer tests ---

TEST_F(LayerServiceTest, ShowLayerFound) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("bilbao", "b.geojson", SpatialScale::Regional);
    presentation_.clear();

    auto result = service_->show_layer("bilbao");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "bilbao");
    EXPECT_EQ(result->scale, SpatialScale::Regional);
    EXPECT_EQ(presentation_.rendered_layers().size(), 1);
}

TEST_F(LayerServiceTest, ShowLayerNotFound) {
    auto result = service_->show_layer("nonexistent");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::LayerNotFound);
    EXPECT_EQ(presentation_.errors().size(), 1);
}

// --- remove_layer tests ---

TEST_F(LayerServiceTest, RemoveLayerSuccess) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("to_delete", "x.geojson", SpatialScale::Urban);
    ASSERT_EQ(persistence_.layer_count(), 1);

    auto result = service_->remove_layer("to_delete");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(persistence_.layer_count(), 0);
}

TEST_F(LayerServiceTest, RemoveLayerNotFound) {
    auto result = service_->remove_layer("ghost");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::LayerNotFound);
}

// --- query_features tests ---

TEST_F(LayerServiceTest, QueryFeaturesWithinExtent) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("bilbao", "b.geojson", SpatialScale::Urban);

    // Tight box around Plaza Moyua only.
    BoundingBox tight{
        .south_west = {43.262, -2.936},
        .north_east = {43.264, -2.934},
    };

    auto result = service_->query_features(tight);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
}

TEST_F(LayerServiceTest, QueryFeaturesNoMatch) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("bilbao", "b.geojson", SpatialScale::Urban);

    BoundingBox far{
        .south_west = {40.0, -3.7},
        .north_east = {40.1, -3.6},
    };

    auto result = service_->query_features(far);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

// --- import_gtfs tests ---

TEST_F(LayerServiceTest, ImportGtfsPartitionsFeaturesByGeometryType) {
    // Mix of Points (stops), LineStrings (routes), and a Polygon (should be
    // excluded from both layers).
    std::vector<GeoFeature> features = {
        GeoFeature{
            .id = "stop_a",
            .geometry = Point{{43.26, -2.93}},
            .properties = {{"name", std::string{"Stop A"}}},
        },
        GeoFeature{
            .id = "route_1",
            .geometry = LineString{{{43.26, -2.93}, {43.27, -2.94}}},
            .properties = {{"name", std::string{"Route 1"}}},
        },
        GeoFeature{
            .id = "stop_b",
            .geometry = Point{{43.27, -2.94}},
            .properties = {{"name", std::string{"Stop B"}}},
        },
        GeoFeature{
            .id = "zone_1",
            .geometry = Polygon{{{{43.26, -2.93}, {43.27, -2.94},
                                  {43.27, -2.93}, {43.26, -2.93}}}},
            .properties = {{"name", std::string{"Zone 1"}}},
        },
    };
    ingestion_.set_features(std::move(features));

    auto result = service_->import_gtfs("/tmp/bilbao.zip", "bilbao");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
    EXPECT_EQ((*result)[0], "bilbao_routes");
    EXPECT_EQ((*result)[1], "bilbao_stops");

    // Verify persisted layers have the correct features.
    auto routes = persistence_.find_layer("bilbao_routes");
    ASSERT_TRUE(routes.has_value());
    EXPECT_EQ(routes->features.size(), 1u);  // Only the LineString.
    EXPECT_EQ(routes->features[0].id, "route_1");

    auto stops = persistence_.find_layer("bilbao_stops");
    ASSERT_TRUE(stops.has_value());
    EXPECT_EQ(stops->features.size(), 2u);  // Two Points.

    // Polygon feature should not appear in either layer.
    EXPECT_EQ(persistence_.layer_count(), 2u);
}

TEST_F(LayerServiceTest, ImportGtfsSuccess_SetsUrbanScale) {
    ingestion_.set_features({
        GeoFeature{
            .id = "s1",
            .geometry = Point{{43.0, -2.0}},
            .properties = {},
        },
    });

    auto result = service_->import_gtfs("/tmp/test.zip", "metro");
    ASSERT_TRUE(result.has_value());

    auto stops = persistence_.find_layer("metro_stops");
    ASSERT_TRUE(stops.has_value());
    EXPECT_EQ(stops->scale, SpatialScale::Urban);

    auto routes = persistence_.find_layer("metro_routes");
    ASSERT_TRUE(routes.has_value());
    EXPECT_EQ(routes->scale, SpatialScale::Urban);
}

TEST_F(LayerServiceTest, ImportGtfsIngestionError) {
    ingestion_.set_error(core::ports::IngestionError::SourceNotFound);

    auto result = service_->import_gtfs("/tmp/missing.zip", "ghost");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::IngestionFailed);
    EXPECT_EQ(presentation_.errors().size(), 1u);
    EXPECT_EQ(persistence_.layer_count(), 0u);
}

TEST_F(LayerServiceTest, ImportGtfsDuplicateRoutesLayer) {
    // Pre-seed a routes layer that will collide.
    ingestion_.set_features(bilbao_features());
    Layer existing;
    existing.name = "transit_routes";
    existing.scale = SpatialScale::Urban;
    (void)persistence_.save_layer(existing);

    // Now set features for the GTFS import including a LineString.
    ingestion_.set_features({
        GeoFeature{
            .id = "r1",
            .geometry = LineString{{{43.0, -2.0}, {43.1, -2.1}}},
            .properties = {},
        },
    });

    auto result = service_->import_gtfs("/tmp/data.zip", "transit");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::DuplicateLayer);
    EXPECT_FALSE(presentation_.errors().empty());
}

TEST_F(LayerServiceTest, ImportGtfsDuplicateStopsLayer) {
    // Pre-seed a stops layer that will collide.
    Layer existing;
    existing.name = "transit_stops";
    existing.scale = SpatialScale::Urban;
    (void)persistence_.save_layer(existing);

    // Features with only a Point (stop) — routes layer will save fine,
    // but stops layer will collide.
    ingestion_.set_features({
        GeoFeature{
            .id = "s1",
            .geometry = Point{{43.0, -2.0}},
            .properties = {},
        },
    });

    auto result = service_->import_gtfs("/tmp/data.zip", "transit");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), LayerServiceError::DuplicateLayer);
    // Routes layer should have been saved before the stops collision.
    auto routes = persistence_.find_layer("transit_routes");
    EXPECT_TRUE(routes.has_value());
}

TEST_F(LayerServiceTest, ImportGtfsEmptyFeatureSet) {
    ingestion_.set_features({});

    auto result = service_->import_gtfs("/tmp/empty.zip", "empty");

    // Should succeed with empty layers.
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);

    auto routes = persistence_.find_layer("empty_routes");
    ASSERT_TRUE(routes.has_value());
    EXPECT_TRUE(routes->features.empty());

    auto stops = persistence_.find_layer("empty_stops");
    ASSERT_TRUE(stops.has_value());
    EXPECT_TRUE(stops->features.empty());
}

TEST_F(LayerServiceTest, ImportGtfsPassesSourceToIngestion) {
    ingestion_.set_features({});

    service_->import_gtfs("/data/my_feed.zip", "feed");

    EXPECT_EQ(ingestion_.last_source(), "/data/my_feed.zip");
}

// --- Additional import_layer edge cases ---

TEST_F(LayerServiceTest, ImportLayerSetsCorrectScale) {
    ingestion_.set_features(bilbao_features());

    auto result = service_->import_layer("regions", "r.geojson",
                                         SpatialScale::Regional);
    ASSERT_TRUE(result.has_value());

    auto layer = persistence_.find_layer("regions");
    ASSERT_TRUE(layer.has_value());
    EXPECT_EQ(layer->scale, SpatialScale::Regional);
}

TEST_F(LayerServiceTest, ImportLayerPassesSourceToIngestion) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("test", "my_source.geojson", SpatialScale::Urban);
    EXPECT_EQ(ingestion_.last_source(), "my_source.geojson");
}

TEST_F(LayerServiceTest, ImportLayerPresentationShowsMessage) {
    ingestion_.set_features(bilbao_features());
    service_->import_layer("reported", "s.geojson", SpatialScale::Urban);

    ASSERT_EQ(presentation_.messages().size(), 1u);
    auto msg = presentation_.messages()[0];
    EXPECT_NE(msg.find("reported"), std::string::npos);
}

// --- Additional list_layers tests ---

TEST_F(LayerServiceTest, ListLayersMultipleLayers) {
    ingestion_.set_features(bilbao_features());
    (void)service_->import_layer("alpha", "a.geojson", SpatialScale::Urban);
    (void)service_->import_layer("beta", "b.geojson", SpatialScale::Regional);
    (void)service_->import_layer("gamma", "c.geojson", SpatialScale::Urban);

    auto result = service_->list_layers();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3u);
}

TEST_F(LayerServiceTest, ListLayersPresentsList) {
    ingestion_.set_features(bilbao_features());
    (void)service_->import_layer("x", "x.geojson", SpatialScale::Urban);
    presentation_.clear();

    auto result = service_->list_layers();
    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(presentation_.presented_lists().size(), 1u);
    EXPECT_EQ(presentation_.presented_lists()[0].size(), 1u);
    EXPECT_EQ(presentation_.presented_lists()[0][0], "x");
}

// --- Additional remove_layer tests ---

TEST_F(LayerServiceTest, RemoveLayerShowsMessage) {
    ingestion_.set_features(bilbao_features());
    (void)service_->import_layer("temp", "t.geojson", SpatialScale::Urban);
    presentation_.clear();

    auto result = service_->remove_layer("temp");
    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(presentation_.messages().size(), 1u);
    EXPECT_NE(presentation_.messages()[0].find("temp"), std::string::npos);
}

TEST_F(LayerServiceTest, RemoveLayerNotFoundShowsError) {
    auto result = service_->remove_layer("missing");
    ASSERT_FALSE(result.has_value());

    ASSERT_EQ(presentation_.errors().size(), 1u);
    EXPECT_NE(presentation_.errors()[0].find("missing"), std::string::npos);
}

// --- Additional query_features tests ---

TEST_F(LayerServiceTest, QueryFeaturesAcrossMultipleLayers) {
    // Import two layers with points in the same area.
    ingestion_.set_features({
        GeoFeature{
            .id = "a1",
            .geometry = Point{{43.26, -2.93}},
            .properties = {},
        },
    });
    (void)service_->import_layer("layer_a", "a.geojson", SpatialScale::Urban);

    ingestion_.set_features({
        GeoFeature{
            .id = "b1",
            .geometry = Point{{43.265, -2.935}},
            .properties = {},
        },
    });
    (void)service_->import_layer("layer_b", "b.geojson", SpatialScale::Urban);

    BoundingBox wide{
        .south_west = {43.0, -3.0},
        .north_east = {44.0, -2.0},
    };

    auto result = service_->query_features(wide);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
}

}  // namespace
}  // namespace garraiobide::app
