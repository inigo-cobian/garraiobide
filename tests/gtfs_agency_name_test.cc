#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../src/adapters/ingestion/gtfs/gtfs_parser.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

// =============================================================================
// Property 1: Agency Name Normalization and Layer Naming
// For any agency_name string, normalize_agency_name SHALL produce a string that
// is entirely lowercase with all spaces replaced by underscores, AND the
// resulting layers SHALL be named "{normalized}_routes" and "{normalized}_stops".
// **Validates: Requirements 2.1, 2.3**
// =============================================================================

// --- Test 1: normalize_agency_name produces all-lowercase output ---

TEST(GtfsAgencyNameTest, ProducesAllLowercaseOutput) {
    EXPECT_EQ(normalize_agency_name("EUSKOTREN"), "euskotren");
    EXPECT_EQ(normalize_agency_name("Metro Bilbao"), "metro_bilbao");
    EXPECT_EQ(normalize_agency_name("ABC"), "abc");
    EXPECT_EQ(normalize_agency_name("MiXeD"), "mixed");
}

// --- Test 2: normalize_agency_name replaces spaces with underscores ---

TEST(GtfsAgencyNameTest, ReplacesSpacesWithUnderscores) {
    EXPECT_EQ(normalize_agency_name("Metro Bilbao"), "metro_bilbao");
    EXPECT_EQ(normalize_agency_name("a b c"), "a_b_c");
    EXPECT_EQ(normalize_agency_name("hello world"), "hello_world");
}

// --- Test 3: Multiple spaces become multiple underscores ---

TEST(GtfsAgencyNameTest, MultipleSpacesBecomeMultipleUnderscores) {
    EXPECT_EQ(normalize_agency_name("a  b"), "a__b");
    EXPECT_EQ(normalize_agency_name("hello   world"), "hello___world");
    EXPECT_EQ(normalize_agency_name("  leading"), "__leading");
    EXPECT_EQ(normalize_agency_name("trailing  "), "trailing__");
}

// --- Test 4: Empty string returns empty string ---

TEST(GtfsAgencyNameTest, EmptyStringReturnsEmpty) {
    EXPECT_EQ(normalize_agency_name(""), "");
}

// --- Test 5: Already lowercase with no spaces returns unchanged ---

TEST(GtfsAgencyNameTest, AlreadyNormalizedReturnsUnchanged) {
    EXPECT_EQ(normalize_agency_name("euskotren"), "euskotren");
    EXPECT_EQ(normalize_agency_name("metro_bilbao"), "metro_bilbao");
    EXPECT_EQ(normalize_agency_name("abc123"), "abc123");
}

// --- Test 6: Mixed case with spaces: "Euskotren Trena" → "euskotren_trena" ---

TEST(GtfsAgencyNameTest, MixedCaseWithSpaces) {
    EXPECT_EQ(normalize_agency_name("Euskotren Trena"), "euskotren_trena");
    EXPECT_EQ(normalize_agency_name("Bilbobus Urbano"), "bilbobus_urbano");
    EXPECT_EQ(normalize_agency_name("San Sebastian Bus"), "san_sebastian_bus");
}

// --- Test 7: parse_gtfs_feed produces layers named "{normalized}_routes" and
//             "{normalized}_stops" ---

TEST(GtfsAgencyNameTest, ParseGtfsFeedProducesCorrectLayerNames) {
    GtfsFeed feed;
    // Minimal agency row
    CsvRow agency_row;
    agency_row["agency_id"] = "1";
    agency_row["agency_name"] = "Euskotren Trena";
    agency_row["agency_url"] = "http://euskotren.eus";
    agency_row["agency_timezone"] = "Europe/Madrid";
    feed.agency.push_back(agency_row);

    // Minimal stop so the feed has some content
    CsvRow stop_row;
    stop_row["stop_id"] = "S001";
    stop_row["stop_name"] = "Abando";
    stop_row["stop_lat"] = "43.26271";
    stop_row["stop_lon"] = "-2.93467";
    feed.stops.push_back(stop_row);

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->routes.name, "euskotren_trena_routes");
    EXPECT_EQ(result->stops.name, "euskotren_trena_stops");
}

TEST(GtfsAgencyNameTest, ParseGtfsFeedWithUpperCaseAgency) {
    GtfsFeed feed;
    CsvRow agency_row;
    agency_row["agency_id"] = "2";
    agency_row["agency_name"] = "METRO BILBAO";
    agency_row["agency_url"] = "http://metrobilbao.eus";
    agency_row["agency_timezone"] = "Europe/Madrid";
    feed.agency.push_back(agency_row);

    CsvRow stop_row;
    stop_row["stop_id"] = "M001";
    stop_row["stop_name"] = "Moyua";
    stop_row["stop_lat"] = "43.26300";
    stop_row["stop_lon"] = "-2.93500";
    feed.stops.push_back(stop_row);

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->routes.name, "metro_bilbao_routes");
    EXPECT_EQ(result->stops.name, "metro_bilbao_stops");
}

TEST(GtfsAgencyNameTest, ParseGtfsFeedFallsBackToGtfsPrefix) {
    // When no agency is provided, default prefix is "gtfs"
    GtfsFeed feed;

    CsvRow stop_row;
    stop_row["stop_id"] = "X001";
    stop_row["stop_name"] = "Test Stop";
    stop_row["stop_lat"] = "43.0";
    stop_row["stop_lon"] = "-2.0";
    feed.stops.push_back(stop_row);

    auto result = parse_gtfs_feed(feed);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->routes.name, "gtfs_routes");
    EXPECT_EQ(result->stops.name, "gtfs_stops");
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
