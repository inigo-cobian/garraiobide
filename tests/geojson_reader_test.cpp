#include <gtest/gtest.h>
#include "io/geojson_reader.hpp"

#include <nlohmann/json.hpp>

TEST(GeojsonReaderTest, read_emptyFile) {
    EXPECT_THROW({ io::GeoJsonReader::read("files/empty.geojson"); }, std::runtime_error);
}

TEST(GeojsonReaderTest, read_validFile) {
    auto result = io::GeoJsonReader::read("files/test.geojson");

    ASSERT_EQ(result.size(), 2);

    EXPECT_EQ(result["type"], "FeatureCollection");
    EXPECT_FALSE(result["features"].empty());
}