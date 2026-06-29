#pragma once
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>

#include "log_level.hpp"

namespace core {
    enum LaunchMode {
        Run, Ingest, Stats
    };

    class StartupConfig {
    protected:
        LaunchMode mode;
        LogLevel logLevel;

        // Database credentials
        std::string mongoUser;
        std::string mongoPass;
        std::string mongoUrl;
        std::string pgUser;
        std::string pgPass;
        std::string pgUrl;

    public:
        StartupConfig() = default;

        ~StartupConfig() = default;

        LaunchMode getMode();

        LogLevel getLogLevel();

        void initializeLogger(LogLevel level);

        [[nodiscard]] std::string getMongoUri() const;

        [[nodiscard]] std::string getPostgresUri() const;

        void setMongo(std::string user, std::string pass, std::string url);

        void setPostgres(std::string user, std::string pass, std::string url);
    };

    class RunConfig : public StartupConfig {
        // empty
    public:
        RunConfig();

        ~RunConfig() = default;
    };

    class IngestConfig : public StartupConfig {
        std::string name;
        std::string type;
        std::string url;
        std::string credentials; // TODO credentials should be managed according to the credential needs
    public:
        IngestConfig();

        ~IngestConfig() = default;

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

        ~StatsConfig() = default;
    };

    using ConfigVariant = std::variant<RunConfig, IngestConfig, StatsConfig>;

    class Args {
    public:
        [[nodiscard]] static std::expected<ConfigVariant, std::runtime_error> parse_args(
            int argc, char *argv[]);
    };
}
