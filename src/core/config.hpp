#pragma once
#include "log_level.hpp"
#include <string>

namespace core {
    enum LaunchMode {
        Run, Ingest, Stats
    };

    class StartupConfig {
    protected:
        LaunchMode mode;
        LogLevel logLevel = LogLevel::Info;

        // Database credentials
        std::string mongoUser;
        std::string mongoPass;
        std::string mongoUrl;
        std::string pgHost;
        std::string pgPort;
        std::string pgUser;
        std::string pgPass;

    public:
        StartupConfig() = default;

        ~StartupConfig() = default;

        [[nodiscard]] LaunchMode getMode() const;

        [[nodiscard]] LogLevel getLogLevel() const;

        void initializeLogger(LogLevel level);

        [[nodiscard]] std::string getMongoUri() const;

        [[nodiscard]] std::string getPostgresConnectionString() const;

        void setMongo(std::string user, std::string pass, std::string url);

        void setPostgres(std::string host, std::string port, std::string user, std::string pass);
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

        [[nodiscard]] std::string getName() const;

        [[nodiscard]] std::string getType() const;

        [[nodiscard]] std::string getUrl() const;

        [[nodiscard]] std::string getCredentials() const;

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
}
