#ifndef GARRAIOBIDE_APP_APP_CONFIG_H
#define GARRAIOBIDE_APP_APP_CONFIG_H

#include <cstdint>
#include <string>

namespace garraiobide::app {

struct AppConfig {
    // Path to the JSON configuration file.
    std::string config_path = "config.json";

    // Logging level (e.g. "trace", "debug", "info", "warn", "error").
    std::string log_level = "info";

    // MongoDB connection parameters.
    std::string mongo_host = "localhost";
    std::uint16_t mongo_port = 27017;
    std::string mongo_user;
    std::string mongo_pass;
};

}  // namespace garraiobide::app

#endif  // GARRAIOBIDE_APP_APP_CONFIG_H
