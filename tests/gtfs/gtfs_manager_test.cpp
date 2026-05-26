#include <gtest/gtest.h>
#include "gtfs/gtfs_manager.hpp"

// source: https://github.com/google/transit/blob/master/gtfs/spec/en/examples/sample-feed-1.zip
const std::string sampleFeed("files/sample-feed-1.zip");

TEST(GtfsManagerTest, loadFeedValid) {
    gtfs::GtfsManager manager;

    EXPECT_NO_THROW(manager.load_feed(sampleFeed));
    EXPECT_EQ(manager.get_stops().size(), 9);
    EXPECT_EQ(manager.get_agencies().size(), 1);
    EXPECT_EQ(manager.get_routes().size(), 5);
    EXPECT_TRUE(manager.get_shapes()->empty());
    EXPECT_EQ(manager.get_trips().size(), 11);
    EXPECT_EQ(manager.get_stop_times().size(), 28);
}