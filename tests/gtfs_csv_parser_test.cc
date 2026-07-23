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

}  // namespace
}  // namespace garraiobide::adapters::ingestion::gtfs
