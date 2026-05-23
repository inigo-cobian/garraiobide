#include <gtest/gtest.h>
#include "io/csv_reader.hpp"

#include <string>
#include <vector>
#include <unordered_map>

using namespace io;

// Helper to compare two row maps (optional, improves error messages)
void ExpectRowEquals(const std::unordered_map<std::string, std::string>& actual,
                     const std::unordered_map<std::string, std::string>& expected) {
    EXPECT_EQ(actual, expected);
}

TEST(CsvReaderTest, EmptyFileReturnsEmptyVector) {
    std::string_view empty = "";
    auto result = CsvReader::parse_file(empty, ',', {"col1"});
    EXPECT_TRUE(result.empty());
}

TEST(CsvReaderTest, HeaderOnlyNoDataRows) {
    std::string_view csv = "Name,Age\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Age"});
    EXPECT_TRUE(result.empty());
}

TEST(CsvReaderTest, SimpleCsvWithoutQuotes) {
    std::string_view csv = "Name,Age\nAlice,30\nBob,25\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Age"});

    ASSERT_EQ(result.size(), 2);

    EXPECT_EQ(result[0]["Name"], "Alice");
    EXPECT_EQ(result[0]["Age"], "30");

    EXPECT_EQ(result[1]["Name"], "Bob");
    EXPECT_EQ(result[1]["Age"], "25");
}

TEST(CsvReaderTest, FieldsContainCommasInsideQuotes) {
    std::string_view csv = "Name,Description\nAlice,\"Engineer, writer\"\nBob,\"Manager\"\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Description"});

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]["Description"], "Engineer, writer");
    EXPECT_EQ(result[1]["Description"], "Manager");
}

TEST(CsvReaderTest, EscapedQuotesInsideQuotedField) {
    // Two double quotes "" represent a single escaped quote inside a quoted field
    std::string_view csv = "Name,Quote\nAlice,\"She said \"\"Hello\"\"\"\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Quote"});

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["Quote"], "She said \"Hello\"");
}

TEST(CsvReaderTest, EmptyFieldsAndTrailingDelimiter) {
    std::string_view csv = "A,B,C\n,value,\nend,,end2\n";
    auto result = CsvReader::parse_file(csv, ',', {"A", "B", "C"});

    ASSERT_EQ(result.size(), 2);

    // First data row: [ "", "value", "" ]
    EXPECT_EQ(result[0]["A"], "");
    EXPECT_EQ(result[0]["B"], "value");
    EXPECT_EQ(result[0]["C"], "");

    // Second data row: [ "end", "", "end2" ]
    EXPECT_EQ(result[1]["A"], "end");
    EXPECT_EQ(result[1]["B"], "");
    EXPECT_EQ(result[1]["C"], "end2");
}

TEST(CsvReaderTest, SkipEmptyLines) {
    std::string_view csv = "X,Y\n\n1,2\n\n\n3,4\n";
    auto result = CsvReader::parse_file(csv, ',', {"X", "Y"});

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]["X"], "1");
    EXPECT_EQ(result[0]["Y"], "2");
    EXPECT_EQ(result[1]["X"], "3");
    EXPECT_EQ(result[1]["Y"], "4");
}

TEST(CsvReaderTest, RequestedColumnsSubset) {
    std::string_view csv = "ID,Name,Age,City\n1,Alice,30,NYC\n2,Bob,25,LA\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "City"});

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].size(), 2); // only requested columns
    EXPECT_EQ(result[0]["Name"], "Alice");
    EXPECT_EQ(result[0]["City"], "NYC");

    EXPECT_EQ(result[1]["Name"], "Bob");
    EXPECT_EQ(result[1]["City"], "LA");
}

TEST(CsvReaderTest, MissingRequestedColumn) {
    std::string_view csv = "Name,Age\nAlice,30\n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Salary"});

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["Name"], "Alice");
    EXPECT_EQ(result[0]["Salary"], ""); // missing column → empty string
}

TEST(CsvReaderTest, DifferentDelimiterSemicolon) {
    std::string_view csv = "Name;Age\nAlice;30\nBob;25\n";
    auto result = CsvReader::parse_file(csv, ';', {"Name", "Age"});

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]["Name"], "Alice");
    EXPECT_EQ(result[0]["Age"], "30");
    EXPECT_EQ(result[1]["Name"], "Bob");
    EXPECT_EQ(result[1]["Age"], "25");
}

TEST(CsvReaderTest, DifferentDelimiterTab) {
    std::string_view csv = "Name\tAge\nAlice\t30\nBob\t25\n";
    auto result = CsvReader::parse_file(csv, '\t', {"Name", "Age"});

    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]["Name"], "Alice");
    EXPECT_EQ(result[0]["Age"], "30");
    EXPECT_EQ(result[1]["Name"], "Bob");
    EXPECT_EQ(result[1]["Age"], "25");
}

TEST(CsvReaderTest, LeadingAndTrailingSpacesArePreserved) {
    std::string_view csv = "Name,Value\n  Alice , 42 \n";
    auto result = CsvReader::parse_file(csv, ',', {"Name", "Value"});

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["Name"], "  Alice ");
    EXPECT_EQ(result[0]["Value"], " 42 ");
}

TEST(CsvReaderTest, RowHasMoreFieldsThanHeaderIgnoresExtra) {
    std::string_view csv = "A,B\n1,2,3\n";
    auto result = CsvReader::parse_file(csv, ',', {"A", "B"});

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["A"], "1");
    EXPECT_EQ(result[0]["B"], "2");
    // Extra field "3" is ignored because header has only two columns
}

TEST(CsvReaderTest, RowHasFewerFieldsThanHeaderMissingFieldsAreEmpty) {
    std::string_view csv = "A,B,C\n1,2\n";
    auto result = CsvReader::parse_file(csv, ',', {"A", "B", "C"});

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["A"], "1");
    EXPECT_EQ(result[0]["B"], "2");
    EXPECT_EQ(result[0]["C"], ""); // missing field becomes empty string
}
