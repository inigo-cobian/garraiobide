#include <gtest/gtest.h>

#include "../src/app/parse_args.h"

namespace garraiobide::app {
namespace {

// Helper to build an argv-style array from initializer list of C-strings.
// taywee/args expects argv[0] to be the program name.
class ArgvBuilder {
   public:
    explicit ArgvBuilder(std::initializer_list<const char*> args)
        : args_(args) {}

    int argc() const { return static_cast<int>(args_.size()); }

    // Returns a non-const char** suitable for parse_args.
    char** argv() { return const_cast<char**>(args_.data()); }

   private:
    std::vector<const char*> args_;
};

TEST(ParseArgsTest, DefaultsWhenNoFlagsProvided) {
    ArgvBuilder args({"garraiobide"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->config_path, "config.json");
    EXPECT_EQ(result->log_level, "info");
    EXPECT_EQ(result->mongo_host, "localhost");
    EXPECT_EQ(result->mongo_port, 27017);
    EXPECT_EQ(result->mongo_user, "");
    EXPECT_EQ(result->mongo_pass, "");
}

TEST(ParseArgsTest, ConfigPathShortFlag) {
    ArgvBuilder args({"garraiobide", "-c", "/etc/app.json"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->config_path, "/etc/app.json");
}

TEST(ParseArgsTest, ConfigPathLongFlag) {
    ArgvBuilder args({"garraiobide", "--config", "/tmp/custom.json"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->config_path, "/tmp/custom.json");
}

TEST(ParseArgsTest, LogLevelShortFlag) {
    ArgvBuilder args({"garraiobide", "-l", "debug"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->log_level, "debug");
}

TEST(ParseArgsTest, LogLevelLongFlag) {
    ArgvBuilder args({"garraiobide", "--log-level", "error"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->log_level, "error");
}

TEST(ParseArgsTest, MongoHostShortFlag) {
    ArgvBuilder args({"garraiobide", "-H", "db.prod.internal"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_host, "db.prod.internal");
}

TEST(ParseArgsTest, MongoHostLongFlag) {
    ArgvBuilder args({"garraiobide", "--mongo-host", "192.168.1.100"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_host, "192.168.1.100");
}

TEST(ParseArgsTest, MongoPortShortFlag) {
    ArgvBuilder args({"garraiobide", "-P", "27018"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_port, 27018);
}

TEST(ParseArgsTest, MongoPortLongFlag) {
    ArgvBuilder args({"garraiobide", "--mongo-port", "28000"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_port, 28000);
}

TEST(ParseArgsTest, MongoUserShortFlag) {
    ArgvBuilder args({"garraiobide", "-u", "admin"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_user, "admin");
}

TEST(ParseArgsTest, MongoUserLongFlag) {
    ArgvBuilder args({"garraiobide", "--mongo-user", "root"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_user, "root");
}

TEST(ParseArgsTest, MongoPassShortFlag) {
    ArgvBuilder args({"garraiobide", "-p", "s3cret"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_pass, "s3cret");
}

TEST(ParseArgsTest, MongoPassLongFlag) {
    ArgvBuilder args({"garraiobide", "--mongo-pass", "hunter2"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mongo_pass, "hunter2");
}

TEST(ParseArgsTest, AllFlagsCombined) {
    ArgvBuilder args({"garraiobide", "-c", "/opt/cfg.json", "-l", "warn",
                      "--mongo-host", "mongo.svc", "--mongo-port", "30000",
                      "-u", "app_user", "-p", "p@ss"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->config_path, "/opt/cfg.json");
    EXPECT_EQ(result->log_level, "warn");
    EXPECT_EQ(result->mongo_host, "mongo.svc");
    EXPECT_EQ(result->mongo_port, 30000);
    EXPECT_EQ(result->mongo_user, "app_user");
    EXPECT_EQ(result->mongo_pass, "p@ss");
}

TEST(ParseArgsTest, HelpFlagReturnsHelpRequested) {
    ArgvBuilder args({"garraiobide", "--help"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseResult::kHelpRequested);
}

TEST(ParseArgsTest, HelpShortFlagReturnsHelpRequested) {
    ArgvBuilder args({"garraiobide", "-h"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseResult::kHelpRequested);
}

TEST(ParseArgsTest, UnknownFlagReturnsError) {
    ArgvBuilder args({"garraiobide", "--bogus-flag"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseResult::kError);
}

TEST(ParseArgsTest, InvalidPortReturnsError) {
    ArgvBuilder args({"garraiobide", "--mongo-port", "not_a_number"});
    auto result = parse_args(args.argc(), args.argv());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ParseResult::kError);
}

}  // namespace
}  // namespace garraiobide::app
