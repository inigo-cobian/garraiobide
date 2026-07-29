#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../src/adapters/ingestion/gtfs/csv_parser.h"

namespace garraiobide::adapters::ingestion::gtfs {
namespace {

// =============================================================================
// Property 7: Coordinate Mapping (subset — field extraction fidelity)
// For any GTFS coordinate pair (stop_lat/stop_lon or shape_pt_lat/shape_pt_lon),
// the values are extracted correctly and can be mapped to coordinates.
// **Validates: Requirements 6.2, 6.3**
// =============================================================================

// --- Test 1: Basic CSV with header + rows → correct field mapping ---

TEST(GtfsCsvParserTest, BasicHeaderAndRows_CorrectFieldMapping) {
    std::string csv = "name,age,city\nAlice,30,Bilbao\nBob,25,Donostia\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].at("name"), "Alice");
    EXPECT_EQ(rows[0].at("age"), "30");
    EXPECT_EQ(rows[0].at("city"), "Bilbao");

    EXPECT_EQ(rows[1].at("name"), "Bob");
    EXPECT_EQ(rows[1].at("age"), "25");
    EXPECT_EQ(rows[1].at("city"), "Donostia");
}

// --- Test 2: Quoted fields with embedded commas ---

TEST(GtfsCsvParserTest, QuotedFieldsWithEmbeddedCommas) {
    std::string csv = "id,description,value\n1,\"has, comma\",100\n2,\"another, one, here\",200\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].at("id"), "1");
    EXPECT_EQ(rows[0].at("description"), "has, comma");
    EXPECT_EQ(rows[0].at("value"), "100");

    EXPECT_EQ(rows[1].at("id"), "2");
    EXPECT_EQ(rows[1].at("description"), "another, one, here");
    EXPECT_EQ(rows[1].at("value"), "200");
}

// --- Test 3: CRLF line endings ---

TEST(GtfsCsvParserTest, CrlfLineEndings) {
    std::string csv = "a,b,c\r\n1,2,3\r\n4,5,6\r\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].at("a"), "1");
    EXPECT_EQ(rows[0].at("b"), "2");
    EXPECT_EQ(rows[0].at("c"), "3");

    EXPECT_EQ(rows[1].at("a"), "4");
    EXPECT_EQ(rows[1].at("b"), "5");
    EXPECT_EQ(rows[1].at("c"), "6");
}

// --- Test 4: Double-quote escaping in quoted fields ---

TEST(GtfsCsvParserTest, DoubleQuoteEscaping) {
    std::string csv = "name,quote\nAlice,\"She said \"\"hello\"\"\"\nBob,\"It's \"\"fine\"\"\"\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].at("name"), "Alice");
    EXPECT_EQ(rows[0].at("quote"), "She said \"hello\"");

    EXPECT_EQ(rows[1].at("name"), "Bob");
    EXPECT_EQ(rows[1].at("quote"), "It's \"fine\"");
}

// --- Test 5: Empty trailing lines are skipped ---

TEST(GtfsCsvParserTest, EmptyTrailingLinesSkipped) {
    std::string csv = "x,y\n1,2\n3,4\n\n\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);

    EXPECT_EQ(rows[0].at("x"), "1");
    EXPECT_EQ(rows[0].at("y"), "2");

    EXPECT_EQ(rows[1].at("x"), "3");
    EXPECT_EQ(rows[1].at("y"), "4");
}

// --- Test 6: GTFS-style CSV with stop_lat/stop_lon field extraction ---

TEST(GtfsCsvParserTest, GtfsStopLatLon_CoordinateFieldExtraction) {
    std::string csv =
        "stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station\n"
        "S001,Plaza Moyua,43.26271,-2.93467,0,\n"
        "S002,Abando,43.26050,-2.92485,1,\n"
        "S003,Abando Platform 1,43.26055,-2.92490,0,S002\n";

    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 3u);

    // Verify stop_lat and stop_lon are extracted as string values correctly
    EXPECT_EQ(rows[0].at("stop_id"), "S001");
    EXPECT_EQ(rows[0].at("stop_name"), "Plaza Moyua");
    EXPECT_EQ(rows[0].at("stop_lat"), "43.26271");
    EXPECT_EQ(rows[0].at("stop_lon"), "-2.93467");
    EXPECT_EQ(rows[0].at("location_type"), "0");
    EXPECT_EQ(rows[0].at("parent_station"), "");

    EXPECT_EQ(rows[1].at("stop_id"), "S002");
    EXPECT_EQ(rows[1].at("stop_lat"), "43.26050");
    EXPECT_EQ(rows[1].at("stop_lon"), "-2.92485");
    EXPECT_EQ(rows[1].at("location_type"), "1");

    EXPECT_EQ(rows[2].at("stop_id"), "S003");
    EXPECT_EQ(rows[2].at("stop_lat"), "43.26055");
    EXPECT_EQ(rows[2].at("stop_lon"), "-2.92490");
    EXPECT_EQ(rows[2].at("parent_station"), "S002");

    // Verify coordinate mapping: stop_lat → latitude, stop_lon → longitude
    // These fields should parse to doubles correctly for coordinate construction
    double lat0 = std::stod(rows[0].at("stop_lat"));
    double lon0 = std::stod(rows[0].at("stop_lon"));
    EXPECT_DOUBLE_EQ(lat0, 43.26271);
    EXPECT_DOUBLE_EQ(lon0, -2.93467);

    double lat1 = std::stod(rows[1].at("stop_lat"));
    double lon1 = std::stod(rows[1].at("stop_lon"));
    EXPECT_DOUBLE_EQ(lat1, 43.26050);
    EXPECT_DOUBLE_EQ(lon1, -2.92485);
}

// --- Test 7: GTFS-style CSV with shape_pt_lat/shape_pt_lon field extraction ---

TEST(GtfsCsvParserTest, GtfsShapePtLatLon_CoordinateFieldExtraction) {
    std::string csv =
        "shape_id,shape_pt_lat,shape_pt_lon,shape_pt_sequence\n"
        "SH1,43.26271,-2.93467,1\n"
        "SH1,43.26350,-2.93100,2\n"
        "SH1,43.26500,-2.92800,3\n"
        "SH2,43.25000,-2.94000,1\n";

    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 4u);

    // Verify shape_pt_lat and shape_pt_lon are extracted correctly
    EXPECT_EQ(rows[0].at("shape_id"), "SH1");
    EXPECT_EQ(rows[0].at("shape_pt_lat"), "43.26271");
    EXPECT_EQ(rows[0].at("shape_pt_lon"), "-2.93467");
    EXPECT_EQ(rows[0].at("shape_pt_sequence"), "1");

    EXPECT_EQ(rows[1].at("shape_pt_lat"), "43.26350");
    EXPECT_EQ(rows[1].at("shape_pt_lon"), "-2.93100");
    EXPECT_EQ(rows[1].at("shape_pt_sequence"), "2");

    EXPECT_EQ(rows[2].at("shape_pt_lat"), "43.26500");
    EXPECT_EQ(rows[2].at("shape_pt_lon"), "-2.92800");
    EXPECT_EQ(rows[2].at("shape_pt_sequence"), "3");

    EXPECT_EQ(rows[3].at("shape_id"), "SH2");
    EXPECT_EQ(rows[3].at("shape_pt_lat"), "43.25000");
    EXPECT_EQ(rows[3].at("shape_pt_lon"), "-2.94000");
    EXPECT_EQ(rows[3].at("shape_pt_sequence"), "1");

    // Verify coordinate mapping: shape_pt_lat → latitude, shape_pt_lon → longitude
    double lat0 = std::stod(rows[0].at("shape_pt_lat"));
    double lon0 = std::stod(rows[0].at("shape_pt_lon"));
    EXPECT_DOUBLE_EQ(lat0, 43.26271);
    EXPECT_DOUBLE_EQ(lon0, -2.93467);

    double lat2 = std::stod(rows[2].at("shape_pt_lat"));
    double lon2 = std::stod(rows[2].at("shape_pt_lon"));
    EXPECT_DOUBLE_EQ(lat2, 43.26500);
    EXPECT_DOUBLE_EQ(lon2, -2.92800);

    // Verify sequence ordering is preserved
    int seq0 = std::stoi(rows[0].at("shape_pt_sequence"));
    int seq1 = std::stoi(rows[1].at("shape_pt_sequence"));
    int seq2 = std::stoi(rows[2].at("shape_pt_sequence"));
    EXPECT_LT(seq0, seq1);
    EXPECT_LT(seq1, seq2);
}

// =============================================================================
// Property 2: Preservation — Non-BOM CSV Parsing Unchanged
// For any CSV content that does NOT start with the UTF-8 BOM sequence,
// the parse_csv() function preserves existing behavior: empty inputs,
// header-only files, LF-only endings, quoting, and multi-column data.
// **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5**
// =============================================================================

TEST(GtfsCsvParserPreservationTest, EmptyInput_ReturnsEmptyVector) {
    // Requirement 3.4: Empty CSV input returns empty result set
    std::string csv = "";
    auto rows = parse_csv(csv);
    EXPECT_TRUE(rows.empty());
}

TEST(GtfsCsvParserPreservationTest, HeaderOnly_ReturnsEmptyVector) {
    // Requirement 3.1: Header-only CSV (no data rows) returns empty vector
    std::string csv = "id,name,value\n";
    auto rows = parse_csv(csv);
    EXPECT_TRUE(rows.empty());
}

TEST(GtfsCsvParserPreservationTest, LfOnlyLineEndings_ParsesCorrectly) {
    // Requirement 3.1: CSV with only LF endings parses correctly
    std::string csv = "x,y,z\n10,20,30\n40,50,60\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].at("x"), "10");
    EXPECT_EQ(rows[0].at("y"), "20");
    EXPECT_EQ(rows[0].at("z"), "30");
    EXPECT_EQ(rows[1].at("x"), "40");
    EXPECT_EQ(rows[1].at("y"), "50");
    EXPECT_EQ(rows[1].at("z"), "60");
}

TEST(GtfsCsvParserPreservationTest, MixedQuotingStyles_ParsesCorrectly) {
    // Requirement 3.2: Quoted fields with commas and escaped quotes
    std::string csv = "id,desc,val\n1,\"has, comma\",100\n2,plain,200\n3,\"with \"\"quotes\"\"\",300\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0].at("id"), "1");
    EXPECT_EQ(rows[0].at("desc"), "has, comma");
    EXPECT_EQ(rows[0].at("val"), "100");
    EXPECT_EQ(rows[1].at("id"), "2");
    EXPECT_EQ(rows[1].at("desc"), "plain");
    EXPECT_EQ(rows[1].at("val"), "200");
    EXPECT_EQ(rows[2].at("id"), "3");
    EXPECT_EQ(rows[2].at("desc"), "with \"quotes\"");
    EXPECT_EQ(rows[2].at("val"), "300");
}

TEST(GtfsCsvParserPreservationTest, SecondColumnUnaffected_AlwaysParsesCorrectly) {
    // Requirement 3.5: Second and subsequent columns are always correct
    std::string csv = "first,second,third\nalpha,beta,gamma\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].at("first"), "alpha");
    EXPECT_EQ(rows[0].at("second"), "beta");
    EXPECT_EQ(rows[0].at("third"), "gamma");
}

// =============================================================================
// Property 1: Bug Condition — BOM Corrupts First Header Field
// CSV content prefixed with UTF-8 BOM (\xEF\xBB\xBF) should still allow
// clean header lookups. On unfixed code, the BOM bytes become part of the
// first header field name, causing std::out_of_range on lookup.
// **Validates: Requirements 1.1, 1.2, 1.3**
// =============================================================================

TEST(GtfsCsvParserBomTest, BomSimpleCsv_FirstHeaderLookupSucceeds) {
    // BOM + simple two-column CSV
    std::string csv = "\xEF\xBB\xBF" "id,name\n1,Alice";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 1u);
    // On unfixed code, the key is "\xEF\xBB\xBFid" so .at("id") throws out_of_range
    EXPECT_EQ(rows[0].at("id"), "1");
    EXPECT_EQ(rows[0].at("name"), "Alice");
}

TEST(GtfsCsvParserBomTest, BomGtfsStops_StopIdLookupSucceeds) {
    // BOM + GTFS-style stops.txt content
    std::string csv = "\xEF\xBB\xBF" "stop_id,stop_name,stop_lat,stop_lon\nS001,Moyua,43.26,-2.93";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 1u);
    // On unfixed code, the key is "\xEF\xBB\xBFstop_id" so .at("stop_id") throws
    EXPECT_EQ(rows[0].at("stop_id"), "S001");
    EXPECT_EQ(rows[0].at("stop_name"), "Moyua");
    EXPECT_EQ(rows[0].at("stop_lat"), "43.26");
    EXPECT_EQ(rows[0].at("stop_lon"), "-2.93");
}

TEST(GtfsCsvParserBomTest, BomWithCrlf_FirstHeaderLookupSucceeds) {
    // BOM + CRLF line endings
    std::string csv = "\xEF\xBB\xBF" "a,b\r\n1,2\r\n";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 1u);
    // On unfixed code, the key is "\xEF\xBB\xBFa" so .at("a") throws
    EXPECT_EQ(rows[0].at("a"), "1");
    EXPECT_EQ(rows[0].at("b"), "2");
}

TEST(GtfsCsvParserBomTest, BomGtfsRoutes_RouteIdLookupSucceeds) {
    // BOM + GTFS-style routes.txt content
    std::string csv = "\xEF\xBB\xBF" "route_id,route_short_name\nR1,L1";
    auto rows = parse_csv(csv);

    ASSERT_EQ(rows.size(), 1u);
    // On unfixed code, the key is "\xEF\xBB\xBFroute_id" so .at("route_id") throws
    EXPECT_EQ(rows[0].at("route_id"), "R1");
    EXPECT_EQ(rows[0].at("route_short_name"), "L1");
}

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
