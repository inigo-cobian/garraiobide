#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "src/adapters/persistence/file_persistence_adapter.h"
#include "src/core/domain/bounding_box.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/layer.h"
#include "src/core/domain/properties.h"
#include "src/core/ports/persistence_port.h"

namespace garraiobide::adapters::persistence {
namespace {

using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::GeoFeature;
using core::domain::Geometry;
using core::domain::Layer;
using core::domain::LineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::Properties;
using core::domain::PropertyValue;
using core::domain::SpatialScale;
using core::ports::PersistenceError;

// --- Test fixture with temp directory management ---

class FilePersistencePropertyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a unique temp directory for each test
        auto base = std::filesystem::temp_directory_path();
        temp_dir_ = base / ("fp_prop_test_" + std::to_string(
                                                   std::random_device{}()));
        std::filesystem::create_directories(temp_dir_);
        adapter_ = std::make_unique<FilePersistenceAdapter>(temp_dir_);
    }

    void TearDown() override {
        adapter_.reset();
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    std::filesystem::path temp_dir_;
    std::unique_ptr<FilePersistenceAdapter> adapter_;
};

// --- Helper: compare features for equivalence ---

bool coordinates_equal(const Coordinate& a, const Coordinate& b) {
    return a.latitude == b.latitude && a.longitude == b.longitude;
}

bool geometry_equal(const Geometry& a, const Geometry& b) {
    if (a.index() != b.index()) return false;

    return std::visit(
        [&b](const auto& ga) -> bool {
            using T = std::decay_t<decltype(ga)>;
            const auto& gb = std::get<T>(b);

            if constexpr (std::is_same_v<T, Point>) {
                return coordinates_equal(ga.position, gb.position);
            } else if constexpr (std::is_same_v<T, LineString>) {
                if (ga.vertices.size() != gb.vertices.size()) return false;
                for (size_t i = 0; i < ga.vertices.size(); ++i) {
                    if (!coordinates_equal(ga.vertices[i], gb.vertices[i]))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, Polygon>) {
                if (ga.rings.size() != gb.rings.size()) return false;
                for (size_t r = 0; r < ga.rings.size(); ++r) {
                    if (ga.rings[r].size() != gb.rings[r].size()) return false;
                    for (size_t i = 0; i < ga.rings[r].size(); ++i) {
                        if (!coordinates_equal(ga.rings[r][i], gb.rings[r][i]))
                            return false;
                    }
                }
                return true;
            }
            return false;
        },
        a);
}

bool properties_equal(const Properties& a, const Properties& b) {
    if (a.size() != b.size()) return false;
    for (const auto& [key, val] : a) {
        auto it = b.find(key);
        if (it == b.end()) return false;
        if (val != it->second) return false;
    }
    return true;
}

bool feature_equal(const GeoFeature& a, const GeoFeature& b) {
    if (a.id != b.id) return false;
    if (!geometry_equal(a.geometry, b.geometry)) return false;
    if (!properties_equal(a.properties, b.properties)) return false;
    return true;
}

bool layer_equal(const Layer& a, const Layer& b) {
    if (a.name != b.name) return false;
    if (a.scale != b.scale) return false;
    if (a.features.size() != b.features.size()) return false;
    for (size_t i = 0; i < a.features.size(); ++i) {
        if (!feature_equal(a.features[i], b.features[i])) return false;
    }
    return true;
}



// =============================================================================
// Property 4: File persistence save/load round trip
// For any valid Layer object, calling save_layer followed by find_layer with
// the same name SHALL return a Layer whose name, scale, feature count, and
// feature data are equivalent to the original.
// **Validates: Requirements 4.1, 4.2**
// =============================================================================

TEST_F(FilePersistencePropertyTest, RoundTrip_PointLayer_Urban) {
    Layer layer{
        .name = "point_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "pt1",
            .geometry = Point{{43.2630, -2.9350}},
            .properties = {{"name", std::string("Plaza Moyua")},
                           {"lines", int64_t{5}},
                           {"accessible", true},
                           {"rating", 4.5}},
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value()) << "save_layer failed";

    auto find_result = adapter_->find_layer("point_layer");
    ASSERT_TRUE(find_result.has_value()) << "find_layer failed";

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

TEST_F(FilePersistencePropertyTest, RoundTrip_LineStringLayer_Regional) {
    Layer layer{
        .name = "route_layer",
        .scale = SpatialScale::Regional,
        .features = {GeoFeature{
            .id = "line1",
            .geometry = LineString{{
                {43.25, -2.95},
                {43.26, -2.93},
                {43.27, -2.91},
            }},
            .properties = {{"route", std::string("A1")},
                           {"length_km", 12.5}},
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto find_result = adapter_->find_layer("route_layer");
    ASSERT_TRUE(find_result.has_value());

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

TEST_F(FilePersistencePropertyTest, RoundTrip_PolygonLayer) {
    Layer layer{
        .name = "zone_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "poly1",
            .geometry = Polygon{{
                {{43.25, -2.95}, {43.25, -2.90}, {43.28, -2.90},
                 {43.28, -2.95}, {43.25, -2.95}},
            }},
            .properties = {{"zone", std::string("residential")},
                           {"population", int64_t{15000}}},
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto find_result = adapter_->find_layer("zone_layer");
    ASSERT_TRUE(find_result.has_value());

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

TEST_F(FilePersistencePropertyTest, RoundTrip_MultiFeatureLayer) {
    Layer layer{
        .name = "mixed_layer",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "f1",
                .geometry = Point{{40.0, -3.7}},
                .properties = {{"type", std::string("stop")}},
            },
            GeoFeature{
                .id = "f2",
                .geometry = LineString{{{40.0, -3.7}, {40.1, -3.6}}},
                .properties = {{"type", std::string("route")}},
            },
            GeoFeature{
                .id = std::nullopt,
                .geometry = Point{{41.0, -2.0}},
                .properties = {},
            },
        },
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto find_result = adapter_->find_layer("mixed_layer");
    ASSERT_TRUE(find_result.has_value());

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

TEST_F(FilePersistencePropertyTest, RoundTrip_EmptyLayer) {
    Layer layer{
        .name = "empty_layer",
        .scale = SpatialScale::Regional,
        .features = {},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto find_result = adapter_->find_layer("empty_layer");
    ASSERT_TRUE(find_result.has_value());

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

TEST_F(FilePersistencePropertyTest, RoundTrip_AllPropertyTypes) {
    Layer layer{
        .name = "props_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "feat_props",
            .geometry = Point{{0.0, 0.0}},
            .properties = {
                {"str_val", std::string("hello world")},
                {"int_val", int64_t{42}},
                {"double_val", 3.14159},
                {"bool_true", true},
                {"bool_false", false},
                {"negative_int", int64_t{-100}},
                {"zero_double", 0.0},
            },
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto find_result = adapter_->find_layer("props_layer");
    ASSERT_TRUE(find_result.has_value());

    EXPECT_TRUE(layer_equal(layer, find_result.value()));
}

// =============================================================================
// Property 5: File persistence list completeness
// For any set of N layers with distinct names that have been saved,
// list_layers SHALL return exactly those N names.
// **Validates: Requirements 4.4**
// =============================================================================

TEST_F(FilePersistencePropertyTest, ListCompleteness_ZeroLayers) {
    auto result = adapter_->list_layers();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0u);
}

TEST_F(FilePersistencePropertyTest, ListCompleteness_OneLayer) {
    Layer layer{.name = "alpha", .scale = SpatialScale::Urban, .features = {}};

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    auto result = adapter_->list_layers();
    ASSERT_TRUE(result.has_value());

    const auto& names = result.value();
    EXPECT_EQ(names.size(), 1u);
    EXPECT_NE(std::find(names.begin(), names.end(), "alpha"), names.end());
}

TEST_F(FilePersistencePropertyTest, ListCompleteness_ThreeLayers) {
    std::vector<std::string> expected_names = {"layer_a", "layer_b", "layer_c"};

    for (const auto& name : expected_names) {
        Layer layer{.name = name, .scale = SpatialScale::Urban, .features = {}};
        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value()) << "Failed to save: " << name;
    }

    auto result = adapter_->list_layers();
    ASSERT_TRUE(result.has_value());

    auto names = result.value();
    std::sort(names.begin(), names.end());
    std::sort(expected_names.begin(), expected_names.end());
    EXPECT_EQ(names, expected_names);
}

TEST_F(FilePersistencePropertyTest, ListCompleteness_FiveLayers) {
    std::vector<std::string> expected_names = {"stops", "routes", "zones",
                                               "buildings", "parks"};

    for (const auto& name : expected_names) {
        Layer layer{.name = name, .scale = SpatialScale::Regional, .features = {}};
        auto save_result = adapter_->save_layer(layer);
        ASSERT_TRUE(save_result.has_value()) << "Failed to save: " << name;
    }

    auto result = adapter_->list_layers();
    ASSERT_TRUE(result.has_value());

    auto names = result.value();
    std::sort(names.begin(), names.end());
    std::sort(expected_names.begin(), expected_names.end());
    EXPECT_EQ(names, expected_names);
}

// =============================================================================
// Property 6: File persistence remove semantics
// For any saved layer, calling remove_layer followed by find_layer SHALL
// return NotFound.
// **Validates: Requirements 4.5**
// =============================================================================

TEST_F(FilePersistencePropertyTest, RemoveSemantics_SingleLayer) {
    Layer layer{
        .name = "to_remove",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "r1",
            .geometry = Point{{43.0, -2.0}},
            .properties = {},
        }},
    };

    auto save_result = adapter_->save_layer(layer);
    ASSERT_TRUE(save_result.has_value());

    // Confirm it exists
    auto found = adapter_->find_layer("to_remove");
    ASSERT_TRUE(found.has_value());

    // Remove it
    auto remove_result = adapter_->remove_layer("to_remove");
    ASSERT_TRUE(remove_result.has_value());

    // Confirm NotFound
    auto after_remove = adapter_->find_layer("to_remove");
    ASSERT_FALSE(after_remove.has_value());
    EXPECT_EQ(after_remove.error(), PersistenceError::NotFound);
}

TEST_F(FilePersistencePropertyTest, RemoveSemantics_DoesNotAffectOtherLayers) {
    Layer layer_a{.name = "keep_me", .scale = SpatialScale::Urban, .features = {}};
    Layer layer_b{.name = "remove_me", .scale = SpatialScale::Urban, .features = {}};

    ASSERT_TRUE(adapter_->save_layer(layer_a).has_value());
    ASSERT_TRUE(adapter_->save_layer(layer_b).has_value());

    // Remove one
    auto remove_result = adapter_->remove_layer("remove_me");
    ASSERT_TRUE(remove_result.has_value());

    // The other still exists
    auto found = adapter_->find_layer("keep_me");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found.value().name, "keep_me");

    // Removed one is gone
    auto gone = adapter_->find_layer("remove_me");
    ASSERT_FALSE(gone.has_value());
    EXPECT_EQ(gone.error(), PersistenceError::NotFound);
}

TEST_F(FilePersistencePropertyTest, RemoveSemantics_RemoveNonExistent) {
    auto result = adapter_->remove_layer("does_not_exist");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), PersistenceError::NotFound);
}

TEST_F(FilePersistencePropertyTest, RemoveSemantics_ListUpdatesAfterRemove) {
    std::vector<std::string> names = {"x", "y", "z"};
    for (const auto& name : names) {
        Layer layer{.name = name, .scale = SpatialScale::Urban, .features = {}};
        ASSERT_TRUE(adapter_->save_layer(layer).has_value());
    }

    // Remove "y"
    ASSERT_TRUE(adapter_->remove_layer("y").has_value());

    auto result = adapter_->list_layers();
    ASSERT_TRUE(result.has_value());

    auto listed = result.value();
    std::sort(listed.begin(), listed.end());
    EXPECT_EQ(listed, (std::vector<std::string>{"x", "z"}));
}

// =============================================================================
// Property 7: Spatial query correctness
// For any set of stored features and any bounding box, query_features SHALL
// return exactly those features with at least one coordinate in the bbox.
// **Validates: Requirements 4.6**
// =============================================================================

TEST_F(FilePersistencePropertyTest, SpatialQuery_PointInsideBbox) {
    Layer layer{
        .name = "spatial_points",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "inside",
                .geometry = Point{{43.26, -2.93}},
                .properties = {},
            },
            GeoFeature{
                .id = "outside",
                .geometry = Point{{40.0, -3.7}},
                .properties = {},
            },
        },
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());

    const auto& features = result.value();
    EXPECT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].id, "inside");
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_LineStringPartiallyInBbox) {
    // A LineString with one vertex inside the bbox and one outside
    Layer layer{
        .name = "spatial_lines",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "partial_line",
                .geometry = LineString{{{43.26, -2.93}, {40.0, -3.7}}},
                .properties = {},
            },
            GeoFeature{
                .id = "fully_outside_line",
                .geometry = LineString{{{40.0, -3.7}, {40.1, -3.6}}},
                .properties = {},
            },
        },
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());

    const auto& features = result.value();
    EXPECT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].id, "partial_line");
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_PolygonVertexInBbox) {
    // A Polygon with one vertex inside the bbox
    Layer layer{
        .name = "spatial_polys",
        .scale = SpatialScale::Urban,
        .features = {
            GeoFeature{
                .id = "poly_in",
                .geometry = Polygon{{
                    {{43.26, -2.93}, {43.26, -2.80}, {43.20, -2.80},
                     {43.20, -2.93}, {43.26, -2.93}},
                }},
                .properties = {},
            },
            GeoFeature{
                .id = "poly_out",
                .geometry = Polygon{{
                    {{40.0, -3.7}, {40.0, -3.6}, {40.1, -3.6},
                     {40.1, -3.7}, {40.0, -3.7}},
                }},
                .properties = {},
            },
        },
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());

    const auto& features = result.value();
    EXPECT_EQ(features.size(), 1u);
    EXPECT_EQ(features[0].id, "poly_in");
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_MultipleLayers) {
    // Features spread across multiple layers
    Layer layer_a{
        .name = "layer_a",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "a_in",
            .geometry = Point{{43.26, -2.93}},
            .properties = {},
        }},
    };
    Layer layer_b{
        .name = "layer_b",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "b_in",
            .geometry = Point{{43.27, -2.94}},
            .properties = {},
        }},
    };
    Layer layer_c{
        .name = "layer_c",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "c_out",
            .geometry = Point{{40.0, -3.7}},
            .properties = {},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer_a).has_value());
    ASSERT_TRUE(adapter_->save_layer(layer_b).has_value());
    ASSERT_TRUE(adapter_->save_layer(layer_c).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());

    const auto& features = result.value();
    EXPECT_EQ(features.size(), 2u);

    // Both a_in and b_in should be present (order doesn't matter)
    std::vector<std::string> ids;
    for (const auto& f : features) {
        if (f.id.has_value()) ids.push_back(f.id.value());
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<std::string>{"a_in", "b_in"}));
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_EmptyBboxNoResults) {
    Layer layer{
        .name = "some_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "pt",
            .geometry = Point{{43.26, -2.93}},
            .properties = {},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    // Bbox far away from any feature
    BoundingBox bbox{
        .south_west = {0.0, 0.0},
        .north_east = {1.0, 1.0},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0u);
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_PointOnBboxBoundary) {
    // A point exactly on the boundary of the bbox should be included
    Layer layer{
        .name = "boundary_layer",
        .scale = SpatialScale::Urban,
        .features = {GeoFeature{
            .id = "on_edge",
            .geometry = Point{{43.23, -2.98}},  // exactly at south_west corner
            .properties = {},
        }},
    };

    ASSERT_TRUE(adapter_->save_layer(layer).has_value());

    BoundingBox bbox{
        .south_west = {43.23, -2.98},
        .north_east = {43.29, -2.90},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].id, "on_edge");
}

TEST_F(FilePersistencePropertyTest, SpatialQuery_NoLayersStored) {
    BoundingBox bbox{
        .south_west = {-90.0, -180.0},
        .north_east = {90.0, 180.0},
    };

    auto result = adapter_->query_features(bbox);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 0u);
}

}  // namespace
}  // namespace garraiobide::adapters::persistence
