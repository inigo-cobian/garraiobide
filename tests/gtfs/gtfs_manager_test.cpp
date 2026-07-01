#include <gtest/gtest.h>
#include "gtfs/gtfs_manager.hpp"

const std::string sampleFeed("https://www.bilbao.eus/opendata/datos/bilbobus-gtfs");

TEST(GtfsManagerTest, loadFeedValid) {
    gtfs::GtfsManager manager;

    EXPECT_NO_THROW(manager.load_feed("test", sampleFeed));
    EXPECT_EQ(manager.get_stops().size(), 537);
    EXPECT_EQ(manager.get_agencies().size(), 1);
    EXPECT_EQ(manager.get_routes().size(), 56);
    EXPECT_FALSE(manager.get_shapes()->empty());
    EXPECT_EQ(manager.get_trips().size(), 189);
    EXPECT_EQ(manager.get_stop_times().size(), 558515);
}