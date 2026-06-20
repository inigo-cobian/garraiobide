#pragma once
#include <string>

namespace core {
    inline struct RunConfig {
        std::string mongoUser;
        std::string mongoPass;
        std::string mongoUrl;
        std::string pgUser;
        std::string pgPass;
        std::string pgUrl;
    } run_config;

    inline struct IngestConfig {
        std::string name;
        std::string type;
        std::string url;
        std::string credentials;
    } ingest_config;

    inline struct StatsConfig {
        // empty
    } stats_config;

    class Args {
    public:
        static int parse_args(int argc, char *argv[]);
    };
}
