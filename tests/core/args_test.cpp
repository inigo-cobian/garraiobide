#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "core/args.hpp"


#include <sstream>
#include <vector>
#include <string>

using namespace core;

// Helper to convert vector<string> to argc/argv for testing
struct ArgvData {
    std::vector<std::string> strings; // owns the actual strings
    std::vector<char *> argv; // pointers into strings
    int argc;

    ArgvData(const std::vector<std::string> &args) {
        strings = args; // copy
        argv.reserve(strings.size());
        for (auto &s: strings) {
            argv.push_back(const_cast<char *>(s.c_str()));
        }
        argc = static_cast<int>(argv.size());
    }
};

// Helper to capture std::cout output (for help flag)
static std::string capture_cout(std::function<void()> func) {
    std::stringstream buffer;
    std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());
    func();
    std::cout.rdbuf(old);
    return buffer.str();
}

TEST(ArgsTest, NoArguments) {
    ArgvData data({"program"});

    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, HelpFlag) {
    ArgvData data({"program", "--help"});

    std::string output = capture_cout([&]() {
        auto result = Args::parse_args(data.argc, data.argv.data());
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().what(), std::string("No mode specified"));
    });
    EXPECT_THAT(output, testing::HasSubstr("Options for Garraiobide."));
    EXPECT_THAT(output, testing::HasSubstr("--help"));
}

TEST(ArgsTest, StatsCommandMinimal) {
    ArgvData data({"program", "stats"});

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<StatsConfig>(*result));
}

TEST(ArgsTest, StatsCommandWithLogLevel) {
    ArgvData data({"program", "stats", "--log", "debug"});

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<StatsConfig>(*result));
}

TEST(ArgsTest, StatsCommandWithMongoAndPg) {
    ArgvData data({
        "program", "stats",
        "--mongo-user", "testUser",
        "--mongo-pass", "testPass",
        "--mongo-url", "mongodb://localhost",
        "--pg-host", "localhost",
        "--pg-port", "5432",
        "--pg-user", "pgUser",
        "--pg-pass", "pgPass"
    });

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<StatsConfig>(*result));
}

TEST(ArgsTest, RunCommandAllRequired) {
    ArgvData data({
        "program", "run",
        "--mongo-user", "mUser",
        "--mongo-pass", "mPass",
        "--mongo-url", "mUrl",
        "--pg-host", "pHost",
        "--pg-port", "pPort",
        "--pg-user", "pUser",
        "--pg-pass", "pPass"
    });

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<RunConfig>(*result));
}

TEST(ArgsTest, RunCommandMissingMongoUser) {
    ArgvData data({
        "program", "run",
        "--mongo-pass", "mPass",
        "--mongo-url", "mUrl",
        "--pg-host", "pHost",
        "--pg-port", "pPort",
        "--pg-user", "pUser",
        "--pg-pass", "pPass"
    });
    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, RunCommandMissingPgPass) {
    ArgvData data({
        "program", "run",
        "--mongo-user", "mUser",
        "--mongo-pass", "mPass",
        "--mongo-url", "mUrl",
        "--pg-host", "pHost",
        "--pg-port", "pPort",
        "--pg-user", "pUser"
        // missing --pg-pass
    });
    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, RunCommandWithLogLevel) {
    ArgvData data({
        "program", "run",
        "--log", "warn",
        "--mongo-user", "mUser",
        "--mongo-pass", "mPass",
        "--mongo-url", "mUrl",
        "--pg-host", "pHost",
        "--pg-port", "pPort",
        "--pg-user", "pUser",
        "--pg-pass", "pPass"
    });
    auto result = Args::parse_args(data.argc, data.argv.data());
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<RunConfig>(*result));
}

TEST(ArgsTest, IngestCommandNoDBArgs) {
    ArgvData data({
        "program", "ingest",
        "--name", "resName",
        "--type", "resType",
        "--url", "resUrl",
        "--creds", "resCreds"
    });


EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);}

TEST(ArgsTest, IngestCommandMissingName) {
    ArgvData data({
        "program", "ingest",
        "--type", "resType",
        "--url", "resUrl",
        "--creds", "resCreds"
    });
    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, IngestCommandMissingCreds) {
    ArgvData data({
        "program", "ingest",
        "--name", "resName",
        "--type", "resType",
        "--url", "resUrl"
        // missing --creds
    });
    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, IngestCommandWithMongoAndPg) {
    ArgvData data({
        "program", "ingest",
        "--name", "resName",
        "--type", "resType",
        "--url", "resUrl",
        "--creds", "resCreds",
        "--mongo-user", "mUser",
        "--mongo-pass", "mPass",
        "--mongo-url", "mUrl",
        "--pg-host", "pHost",
        "--pg-port", "pPort",
        "--pg-user", "pUser",
        "--pg-pass", "pPass"
    });
    auto result = Args::parse_args(data.argc, data.argv.data());
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<IngestConfig>(*result));
}

TEST(ArgsTest, InvalidLogLevel) {
    ArgvData data({"program", "stats", "--log", "invalid"});

    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}

TEST(ArgsTest, LogLevelDefaultInfo) {
    // No --log flag, should default to Info (no exception, parse succeeds)
    ArgvData data({"program", "stats"});

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
}

TEST(ArgsTest, TwoCommands) {
    ArgvData data({"program", "stats", "run"});

    auto result = Args::parse_args(data.argc, data.argv.data());

    EXPECT_FALSE(result.has_value());
    // TODO check why the first letter is missing. Quite a funny bug in this case, but still.
    EXPECT_EQ(result.error().what(), std::string("assed in argument, but no positional arguments were ready to receive it: run"));
}

TEST(ArgsTest, UnknownOption) {
    ArgvData data({"program", "stats", "--unknown"});

    auto result = Args::parse_args(data.argc, data.argv.data());

    EXPECT_FALSE(result.has_value());
    // TODO check why the first letter is missing.
    EXPECT_EQ(result.error().what(), std::string("lag could not be matched: unknown"));
}

TEST(ArgsTest, EmptyLogLevel) {
    ArgvData data({"program", "stats", "--log", ""});

    auto result = Args::parse_args(data.argc, data.argv.data());

    ASSERT_TRUE(result.has_value());
}

TEST(ArgsTest, GlobalOptionsWithoutCommand) {
    ArgvData data({"program", "--mongo-user", "u"});

    EXPECT_THROW(Args::parse_args(data.argc, data.argv.data()), std::runtime_error);
}
