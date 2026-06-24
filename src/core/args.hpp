#pragma once
#include <expected>
#include <stdexcept>
#include <string>

#include "LogLevel.hpp"

namespace core {
    enum LaunchMode {
        Run, Ingest, Stats
    };

    class StartupConfig {
    protected:
        LaunchMode mode;
        LogLevel logLevel;
    public:
        LaunchMode getMode();
        LogLevel getLogLevel();

        void initializeLogger(LogLevel level);
    };

    class RunConfig : public StartupConfig {
        std::string mongoUser;
        std::string mongoPass;
        std::string mongoUrl;
        std::string pgUser;
        std::string pgPass;
        std::string pgUrl;
    public:
        RunConfig();
        [[nodiscard]] std::string getMongoUri();
        [[nodiscard]] std::string getPostgresUri();

        void setMongo(std::string user, std::string pass, std::string url);
        void setPostgres(std::string user, std::string pass, std::string url);
    };

    class IngestConfig : public StartupConfig {
        std::string name;
        std::string type;
        std::string url;
        std::string credentials; // TODO credentials should be managed according to the credential needs
    public:
        IngestConfig();
        [[nodiscard]] std::string getName();
        [[nodiscard]] std::string getType();
        [[nodiscard]] std::string getUrl();
        [[nodiscard]] std::string getCredentials();

        void setName(std::string name);
        void setType(std::string type);
        void setUrl(std::string url);
        void setCredentials(std::string credentials);
    };

    class StatsConfig : public StartupConfig {
        // empty
    public:
        StatsConfig();
    };

    class Args {
    public:
        [[nodiscard]] static std::expected<StartupConfig, std::runtime_error> parse_args(int argc, char *argv[]);
    };
}
