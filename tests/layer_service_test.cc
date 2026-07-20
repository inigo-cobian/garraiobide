#include <gtest/gtest.h>

#include "src/adapters/ingestion/mock_ingestion_adapter.h"
#include "src/adapters/persistence/mock_persistence_adapter.h"
#include "src/adapters/ui/mock_presentation_adapter.h"
#include "src/app/layer_service.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"

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

}  // namespace
}  // namespace garraiobide::app
