#include "core/config.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace core;


TEST(ConfigTest, RunConfigDefaults) {
    RunConfig cfg;
    EXPECT_EQ(cfg.getMode(), LaunchMode::Run);
    EXPECT_EQ(cfg.getLogLevel(), LogLevel::Info);
}

TEST(ConfigTest, StatsConfigDefaults) {
    StatsConfig cfg;
    EXPECT_EQ(cfg.getMode(), LaunchMode::Stats);
    EXPECT_EQ(cfg.getLogLevel(), LogLevel::Info);
}

TEST(ConfigTest, IngestConfigSettersGetters) {
    IngestConfig cfg;
    cfg.setName("test");
    cfg.setType("gtfs");
    cfg.setUrl("http://example.com");
    cfg.setCredentials("user:pass");

    EXPECT_EQ(cfg.getName(), "test");
    EXPECT_EQ(cfg.getType(), "gtfs");
    EXPECT_EQ(cfg.getUrl(), "http://example.com");
    EXPECT_EQ(cfg.getCredentials(), "user:pass");
}

TEST(ConfigTest, StartupConfigMongoUri) {
    StartupConfig cfg;
    cfg.setMongo("myuser", "mypass", "mongo.db:27017");
    EXPECT_EQ(cfg.getMongoUri(), "mongodb://myuser:mypass@mongo.db:27017/?authSource=admin&authMechanism=SCRAM-SHA-256");
}

TEST(ConfigTest, StartupConfigPostgresConnectionString) {
    StartupConfig cfg;
    cfg.setPostgres("pg.host", "5433", "pguser", "pgpwd");
    EXPECT_EQ(cfg.getPostgresConnectionString(), "host=pg.host port=5433 dbname=garraiobide user=pguser password=pgpwd");
}
