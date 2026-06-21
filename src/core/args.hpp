#pragma once
#include <expected>
#include <stdexcept>
#include <string>

namespace core {
    enum LaunchMode {
        Run, Ingest, Stats
    };

    struct StartupConfig {
        LaunchMode mode;
        std::string logLevel;
    };

    struct RunConfig : public StartupConfig {
        std::string mongoUser;
        std::string mongoPass;
        std::string mongoUrl;
        std::string pgUser;
        std::string pgPass;
        std::string pgUrl;
    };

    struct IngestConfig : public StartupConfig {
        std::string name;
        std::string type;
        std::string url;
        std::string credentials;
    };

    struct StatsConfig : public StartupConfig {
        // empty
    };

    class Args {
    public:
        [[nodiscard]] static std::expected<StartupConfig, std::runtime_error> parse_args(int argc, char *argv[]);
    };
}
