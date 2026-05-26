#include <gtest/gtest.h>
#include "core/resource.hpp"

TEST(ResourceTest, createValidResource) {
    auto name = "metro-bilbao";
    auto metroBilbaoGtfsUrl = "https://cms.metrobilbao.eus/get/open_data/horarios/eu";

    core::Resource res(name, metroBilbaoGtfsUrl, core::ResourceType::gtfs);

    ASSERT_EQ(res.get_name(), name);
    ASSERT_EQ(res.get_url(), metroBilbaoGtfsUrl);
    ASSERT_EQ(res.get_url(), metroBilbaoGtfsUrl);
    ASSERT_EQ(res.get_type(), core::ResourceType::gtfs);
    // Should be the same at time of creation
    ASSERT_EQ(res.get_creation_time(), res.get_last_modified());
}

TEST(ResourceTest, getResourceAsJson) {
    auto name = "metro-bilbao";
    auto metroBilbaoGtfsUrl = "https://cms.metrobilbao.eus/get/open_data/horarios/eu";
    core::Resource res(name, metroBilbaoGtfsUrl, core::ResourceType::gtfs);

    auto ret = res.as_json();

    // It would be ideal to be able to directly test a literal json like this, but the timestamp makes it hard
    auto result = R"(
        {"creation_time":1779812896259974122,"last_modified":1779812896259974122,"name":"metro-bilbao","type":"gtfs"}
    )"_json;
    ASSERT_EQ(ret["name"], name);
    ASSERT_EQ(ret["url"], metroBilbaoGtfsUrl);

}
