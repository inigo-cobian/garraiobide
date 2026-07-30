#include <gtest/gtest.h>
#include <algorithm>
#include <random>
#include <string>
#include <vector>
#include "../src/adapters/persistence/mongo_persistence_adapter.h"
#include "../src/core/domain/bounding_box.h"
#include "../src/core/domain/coordinate.h"
#include "../src/core/domain/geo_feature.h"
#include "../src/core/domain/geometry.h"
#include "../src/core/domain/layer.h"
#include "../src/core/domain/properties.h"
#include "../src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {
namespace {

using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::Properties;
using core::domain::SpatialScale;
using core::ports::PersistenceError;

// === Test fixture ===
class MongoPersistenceAdapterTest : public ::testing::Test {
   protected:
    void SetUp() override {
        conn_ = "mongodb://localhost:27017";
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        db_ = "adapter_test_" + std::to_string(dist(gen));
        adapter_ = std::make_unique<MongoPersistenceAdapter>(conn_, db_);
    }
    void TearDown() override {
        adapter_.reset();
        try {
            mongocxx::client c{mongocxx::uri{conn_}};
            c[db_].drop();
        } catch (...) {}
    }
    std::string conn_;
    std::string db_;
    std::unique_ptr<MongoPersistenceAdapter> adapter_;
};

// === Constructor validation ===
TEST(MongoPersistenceAdapterCtorTest, EmptyConnectionStringThrows) {
    EXPECT_THROW(MongoPersistenceAdapter("", "db"), std::invalid_argument);
}
TEST(MongoPersistenceAdapterCtorTest, EmptyDatabaseNameThrows) {
    EXPECT_THROW(MongoPersistenceAdapter("mongodb://localhost:27017", ""), std::invalid_argument);
}
TEST(MongoPersistenceAdapterCtorTest, BothEmptyThrows) {
    EXPECT_THROW(MongoPersistenceAdapter("", ""), std::invalid_argument);
}
TEST(MongoPersistenceAdapterCtorTest, InvalidUriThrows) {
    EXPECT_THROW(MongoPersistenceAdapter("not_a_valid_uri", "db"), std::exception);
}

// === save_layer ===
TEST_F(MongoPersistenceAdapterTest, SaveAndFindSinglePointFeature) {
    Layer layer{.name = "stops", .scale = SpatialScale::Urban,
        .features = {GeoFeature{.id = "s1", .geometry = Point{{43.26, -2.93}},
            .properties = {{"name", std::string("Moyua")}}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("stops");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->features[0].id, "s1");
}
TEST_F(MongoPersistenceAdapterTest, SaveEmptyNameReturnsWriteError) {
    Layer layer{.name = "", .features = {}};
    auto r = adapter_->save_layer(layer);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), PersistenceError::WriteError);
}
TEST_F(MongoPersistenceAdapterTest, SaveDuplicateReturnsDuplicateError) {
    Layer layer{.name = "dup", .features = {}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto r = adapter_->save_layer(layer);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), PersistenceError::DuplicateLayer);
}
TEST_F(MongoPersistenceAdapterTest, SaveEmptyFeaturesRoundTrips) {
    Layer layer{.name = "empty", .scale = SpatialScale::Regional, .features = {}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("empty");
    ASSERT_TRUE(f.has_value());
    EXPECT_TRUE(f->features.empty());
    EXPECT_EQ(f->scale, SpatialScale::Regional);
}
TEST_F(MongoPersistenceAdapterTest, SaveFeatureWithoutId) {
    Layer layer{.name = "no_id", .features = {GeoFeature{
        .id = std::nullopt, .geometry = Point{{1.0, 2.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("no_id");
    ASSERT_TRUE(f.has_value());
    EXPECT_FALSE(f->features[0].id.has_value());
}
TEST_F(MongoPersistenceAdapterTest, SaveMixedGeometries) {
    Layer layer{.name = "mixed", .scale = SpatialScale::Urban, .features = {
        GeoFeature{.id = "pt", .geometry = Point{{43.26, -2.93}}, .properties = {}},
        GeoFeature{.id = "ls", .geometry = LineString{{{43.25, -2.95}, {43.27, -2.91}}},
            .properties = {}},
        GeoFeature{.id = "pg", .geometry = Polygon{{{{43.25, -2.95}, {43.25, -2.90},
            {43.28, -2.90}, {43.28, -2.95}, {43.25, -2.95}}}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("mixed");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->features.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<Point>(f->features[0].geometry));
    EXPECT_TRUE(std::holds_alternative<LineString>(f->features[1].geometry));
    EXPECT_TRUE(std::holds_alternative<Polygon>(f->features[2].geometry));
}
TEST_F(MongoPersistenceAdapterTest, SaveAllPropertyTypes) {
    Layer layer{.name = "props", .features = {GeoFeature{.id = "f1",
        .geometry = Point{{0.0, 0.0}}, .properties = {
            {"s", std::string("hello")}, {"d", 3.14}, {"i", int64_t{42}},
            {"bt", true}, {"bf", false}, {"neg", int64_t{-100}}}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("props");
    ASSERT_TRUE(f.has_value());
    const auto& p = f->features[0].properties;
    EXPECT_EQ(std::get<std::string>(p.at("s")), "hello");
    EXPECT_EQ(std::get<double>(p.at("d")), 3.14);
    EXPECT_EQ(std::get<int64_t>(p.at("i")), 42);
    EXPECT_EQ(std::get<bool>(p.at("bt")), true);
    EXPECT_EQ(std::get<bool>(p.at("bf")), false);
    EXPECT_EQ(std::get<int64_t>(p.at("neg")), -100);
}

// === find_layer ===
TEST_F(MongoPersistenceAdapterTest, FindNonExistentReturnsNotFound) {
    auto r = adapter_->find_layer("ghost");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), PersistenceError::NotFound);
}
TEST_F(MongoPersistenceAdapterTest, FindPreservesFeatureOrder) {
    Layer layer{.name = "ordered", .features = {
        GeoFeature{.id = "a", .geometry = Point{{1.0, 1.0}}, .properties = {}},
        GeoFeature{.id = "b", .geometry = Point{{2.0, 2.0}}, .properties = {}},
        GeoFeature{.id = "c", .geometry = Point{{3.0, 3.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("ordered");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->features.size(), 3u);
    EXPECT_EQ(f->features[0].id, "a");
    EXPECT_EQ(f->features[1].id, "b");
    EXPECT_EQ(f->features[2].id, "c");
}
TEST_F(MongoPersistenceAdapterTest, FindPreservesScale) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "u", .scale = SpatialScale::Urban,
        .features = {}}).has_value());
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "r", .scale = SpatialScale::Regional,
        .features = {}}).has_value());
    EXPECT_EQ(adapter_->find_layer("u")->scale, SpatialScale::Urban);
    EXPECT_EQ(adapter_->find_layer("r")->scale, SpatialScale::Regional);
}

// === list_layers ===
TEST_F(MongoPersistenceAdapterTest, ListEmptyDatabaseReturnsEmpty) {
    auto r = adapter_->list_layers();
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}
TEST_F(MongoPersistenceAdapterTest, ListReturnsSavedNames) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "alpha", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "beta", .features = {}}).has_value());
    auto r = adapter_->list_layers();
    ASSERT_TRUE(r.has_value());
    auto names = r.value();
    std::sort(names.begin(), names.end());
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "alpha");
    EXPECT_EQ(names[1], "beta");
}
TEST_F(MongoPersistenceAdapterTest, ListReflectsRemovals) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "keep", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "drop", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->remove_layer("drop").has_value());
    auto r = adapter_->list_layers();
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 1u);
    EXPECT_EQ(r->at(0), "keep");
}

// === remove_layer ===
TEST_F(MongoPersistenceAdapterTest, RemoveExistingSucceeds) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "rm", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->remove_layer("rm").has_value());
    EXPECT_EQ(adapter_->find_layer("rm").error(), PersistenceError::NotFound);
}
TEST_F(MongoPersistenceAdapterTest, RemoveNonExistentReturnsNotFound) {
    auto r = adapter_->remove_layer("nope");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), PersistenceError::NotFound);
}
TEST_F(MongoPersistenceAdapterTest, RemoveTwiceReturnsNotFoundSecondTime) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "once", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->remove_layer("once").has_value());
    EXPECT_EQ(adapter_->remove_layer("once").error(), PersistenceError::NotFound);
}
TEST_F(MongoPersistenceAdapterTest, RemoveDoesNotAffectOtherLayers) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "a", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "b", .features = {}}).has_value());
    ASSERT_TRUE(adapter_->remove_layer("b").has_value());
    EXPECT_TRUE(adapter_->find_layer("a").has_value());
}
TEST_F(MongoPersistenceAdapterTest, CanReuseNameAfterRemoval) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "reuse", .scale = SpatialScale::Urban,
        .features = {}}).has_value());
    ASSERT_TRUE(adapter_->remove_layer("reuse").has_value());
    Layer v2{.name = "reuse", .scale = SpatialScale::Regional, .features = {}};
    ASSERT_TRUE(adapter_->save_layer(v2).has_value());
    auto f = adapter_->find_layer("reuse");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->scale, SpatialScale::Regional);
}

// === query_features — spatial ===
TEST_F(MongoPersistenceAdapterTest, QueryFindsPointInsideBBox) {
    Layer layer{.name = "q1", .features = {
        GeoFeature{.id = "in", .geometry = Point{{43.26, -2.93}}, .properties = {}},
        GeoFeature{.id = "out", .geometry = Point{{10.0, 10.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox bbox{.south_west = {43.20, -3.00}, .north_east = {43.30, -2.85}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 1u);
    EXPECT_EQ(r->at(0).id, "in");
}
TEST_F(MongoPersistenceAdapterTest, QueryEmptyForDisjointBBox) {
    Layer layer{.name = "q2", .features = {GeoFeature{.id = "p",
        .geometry = Point{{48.85, 2.35}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox bbox{.south_west = {35.5, 139.5}, .north_east = {35.9, 140.0}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}
TEST_F(MongoPersistenceAdapterTest, QueryInvertedLatReturnsEmpty) {
    Layer layer{.name = "q3", .features = {GeoFeature{.id = "x",
        .geometry = Point{{0.0, 0.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox inv{.south_west = {50.0, -10.0}, .north_east = {10.0, 10.0}};
    auto r = adapter_->query_features(inv);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}
TEST_F(MongoPersistenceAdapterTest, QueryInvertedLonReturnsEmpty) {
    Layer layer{.name = "q4", .features = {GeoFeature{.id = "x",
        .geometry = Point{{0.0, 0.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox inv{.south_west = {-10.0, 50.0}, .north_east = {10.0, -50.0}};
    auto r = adapter_->query_features(inv);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}
TEST_F(MongoPersistenceAdapterTest, QueryAcrossMultipleLayers) {
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "la", .features = {GeoFeature{
        .id = "a1", .geometry = Point{{43.26, -2.93}}, .properties = {}}}}).has_value());
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "lb", .features = {GeoFeature{
        .id = "b1", .geometry = Point{{43.27, -2.94}}, .properties = {}}}}).has_value());
    BoundingBox bbox{.south_west = {43.20, -3.00}, .north_east = {43.30, -2.85}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->size(), 2u);
    std::vector<std::string> ids;
    for (const auto& f : *r) if (f.id) ids.push_back(*f.id);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<std::string>{"a1", "b1"}));
}
TEST_F(MongoPersistenceAdapterTest, QueryLineStringIntersecting) {
    Layer layer{.name = "q5", .features = {GeoFeature{.id = "rt",
        .geometry = LineString{{{43.20, -3.00}, {43.30, -2.85}}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox bbox{.south_west = {43.23, -2.96}, .north_east = {43.28, -2.90}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 1u);
    EXPECT_EQ(r->at(0).id, "rt");
}
TEST_F(MongoPersistenceAdapterTest, QueryPolygonIntersecting) {
    Layer layer{.name = "q6", .features = {GeoFeature{.id = "zone",
        .geometry = Polygon{{{{43.24, -2.96}, {43.24, -2.90}, {43.28, -2.90},
            {43.28, -2.96}, {43.24, -2.96}}}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox bbox{.south_west = {43.25, -2.95}, .north_east = {43.27, -2.92}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 1u);
    EXPECT_EQ(r->at(0).id, "zone");
}
TEST_F(MongoPersistenceAdapterTest, QueryEmptyDatabaseReturnsEmpty) {
    BoundingBox world{.south_west = {-90.0, -180.0}, .north_east = {90.0, 180.0}};
    auto r = adapter_->query_features(world);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}
TEST_F(MongoPersistenceAdapterTest, QueryPreservesProperties) {
    Layer layer{.name = "q7", .features = {GeoFeature{.id = "rich",
        .geometry = Point{{5.0, 5.0}}, .properties = {
            {"name", std::string("Station")}, {"count", int64_t{99}},
            {"rate", 4.5}, {"open", true}}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    BoundingBox bbox{.south_west = {4.0, 4.0}, .north_east = {6.0, 6.0}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 1u);
    const auto& p = r->at(0).properties;
    EXPECT_EQ(std::get<std::string>(p.at("name")), "Station");
    EXPECT_EQ(std::get<int64_t>(p.at("count")), 99);
    EXPECT_EQ(std::get<double>(p.at("rate")), 4.5);
    EXPECT_EQ(std::get<bool>(p.at("open")), true);
}
TEST_F(MongoPersistenceAdapterTest, QueryExcludesRemovedLayer) {
    Layer layer{.name = "gone", .features = {GeoFeature{.id = "x",
        .geometry = Point{{5.0, 5.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    ASSERT_TRUE(adapter_->remove_layer("gone").has_value());
    BoundingBox bbox{.south_west = {4.0, 4.0}, .north_east = {6.0, 6.0}};
    auto r = adapter_->query_features(bbox);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}

// === Geometry round-trip details ===
TEST_F(MongoPersistenceAdapterTest, PointCoordinatesPrecise) {
    Layer layer{.name = "prt", .features = {GeoFeature{.id = "p",
        .geometry = Point{{-33.8688, 151.2093}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("prt");
    ASSERT_TRUE(f.has_value());
    auto pt = std::get<Point>(f->features[0].geometry);
    EXPECT_DOUBLE_EQ(pt.position.latitude, -33.8688);
    EXPECT_DOUBLE_EQ(pt.position.longitude, 151.2093);
}
TEST_F(MongoPersistenceAdapterTest, LineStringVerticesPreserved) {
    std::vector<Coordinate> verts = {{40.71, -74.00}, {34.05, -118.24}, {41.88, -87.63}};
    Layer layer{.name = "lrt", .features = {GeoFeature{.id = "l",
        .geometry = LineString{verts}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("lrt");
    ASSERT_TRUE(f.has_value());
    auto ls = std::get<LineString>(f->features[0].geometry);
    ASSERT_EQ(ls.vertices.size(), 3u);
    EXPECT_DOUBLE_EQ(ls.vertices[1].latitude, 34.05);
    EXPECT_DOUBLE_EQ(ls.vertices[1].longitude, -118.24);
}
TEST_F(MongoPersistenceAdapterTest, PolygonWithHolePreserved) {
    std::vector<std::vector<Coordinate>> rings = {
        {{43.24, -2.96}, {43.24, -2.90}, {43.29, -2.90}, {43.29, -2.96}, {43.24, -2.96}},
        {{43.25, -2.95}, {43.28, -2.95}, {43.28, -2.91}, {43.25, -2.91}, {43.25, -2.95}}};
    Layer layer{.name = "phrt", .features = {GeoFeature{.id = "ph",
        .geometry = Polygon{rings}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("phrt");
    ASSERT_TRUE(f.has_value());
    auto poly = std::get<Polygon>(f->features[0].geometry);
    ASSERT_EQ(poly.rings.size(), 2u);
    EXPECT_EQ(poly.rings[0].size(), 5u);
    EXPECT_EQ(poly.rings[1].size(), 5u);
}

// === Edge cases ===
TEST_F(MongoPersistenceAdapterTest, ExtremeCoordinates) {
    Layer layer{.name = "extreme", .features = {
        GeoFeature{.id = "np", .geometry = Point{{90.0, 0.0}}, .properties = {}},
        GeoFeature{.id = "sp", .geometry = Point{{-90.0, 0.0}}, .properties = {}},
        GeoFeature{.id = "dl", .geometry = Point{{0.0, 180.0}}, .properties = {}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("extreme");
    ASSERT_TRUE(f.has_value());
    ASSERT_EQ(f->features.size(), 3u);
    EXPECT_DOUBLE_EQ(std::get<Point>(f->features[0].geometry).position.latitude, 90.0);
    EXPECT_DOUBLE_EQ(std::get<Point>(f->features[1].geometry).position.latitude, -90.0);
}
TEST_F(MongoPersistenceAdapterTest, SpecialCharsInNameAndProperties) {
    Layer layer{.name = "layer (v2) - test", .features = {GeoFeature{.id = "f",
        .geometry = Point{{0.0, 0.0}}, .properties = {
            {"desc", std::string("café & \"quoted\"")},
            {"nl", std::string("a\nb")}}}}};
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("layer (v2) - test");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(std::get<std::string>(f->features[0].properties.at("desc")),
              "café & \"quoted\"");
    EXPECT_EQ(std::get<std::string>(f->features[0].properties.at("nl")), "a\nb");
}
TEST_F(MongoPersistenceAdapterTest, ManyFeaturesRoundTrip) {
    Layer layer{.name = "many", .features = {}};
    for (int i = 0; i < 50; ++i) {
        layer.features.push_back(GeoFeature{.id = "f" + std::to_string(i),
            .geometry = Point{{40.0 + i * 0.01, -3.0 + i * 0.01}},
            .properties = {{"idx", int64_t{i}}}});
    }
    ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    auto f = adapter_->find_layer("many");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->features.size(), 50u);
    EXPECT_EQ(f->features[0].id, "f0");
    EXPECT_EQ(f->features[49].id, "f49");
}

// === Index idempotency ===
TEST_F(MongoPersistenceAdapterTest, SecondAdapterOnSameDbSucceeds) {
    EXPECT_NO_THROW({ MongoPersistenceAdapter second(conn_, db_); });
    // Original still works
    ASSERT_TRUE(adapter_->save_layer(Layer{.name = "post", .features = {}}).has_value());
    EXPECT_TRUE(adapter_->find_layer("post").has_value());
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
