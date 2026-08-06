#include <gtest/gtest.h>

#include <string>
#include <variant>

#include "../src/adapters/ingestion/gtfs/gtfs_ingestion_adapter.h"
#include "../src/core/domain/geometry.h"

namespace garraiobide::tests {
namespace {

using adapters::ingestion::gtfs::GtfsIngestionAdapter;
using core::domain::LineString;
using core::domain::MultiLineString;

/// Metro Bilbao has 1 route with 29 distinct shapes (all trip variants).
/// The route geometry must be a MultiLineString containing all 29 lines.
TEST(GtfsMetroBilbaoShapesTest, RouteContainsAllShapes) {
    GtfsIngestionAdapter adapter;
    std::string zip_path = std::string(TEST_FIXTURES_DIR) + "/metro_bilbao.zip";

    auto result = adapter.load_features(zip_path);
    ASSERT_TRUE(result.has_value()) << "Failed to load metro_bilbao.zip";

    // Find the route feature (not a stop — stops have Point geometry)
    const core::domain::GeoFeature* route_feature = nullptr;
    for (const auto& f : *result) {
        if (std::holds_alternative<MultiLineString>(f.geometry) ||
            std::holds_alternative<LineString>(f.geometry)) {
            route_feature = &f;
            break;
        }
    }
    ASSERT_NE(route_feature, nullptr) << "No route feature found";

    // Must be MultiLineString with all 29 shapes
    ASSERT_TRUE(std::holds_alternative<MultiLineString>(route_feature->geometry))
        << "Expected MultiLineString for route with multiple shapes, got "
        << (std::holds_alternative<LineString>(route_feature->geometry) ? "LineString" : "other");

    const auto& mls = std::get<MultiLineString>(route_feature->geometry);
    EXPECT_EQ(mls.lines.size(), 29u)
        << "Metro Bilbao has 29 unique shapes across all trip variants";

    // Sanity: total points should be ~12982
    std::size_t total_pts = 0;
    for (const auto& line : mls.lines) {
        EXPECT_FALSE(line.empty()) << "No shape line should be empty";
        total_pts += line.size();
    }
    EXPECT_GT(total_pts, 10000u) << "Total shape points should be substantial";
}

}  // namespace
}  // namespace garraiobide::tests
