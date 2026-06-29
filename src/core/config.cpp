#include "config.hpp"

#include "log_level.hpp"

namespace core {
    LaunchMode StartupConfig::getMode() const {
        return mode;
    }

    LogLevel StartupConfig::getLogLevel() const {
        return logLevel;
    }

    void StartupConfig::initializeLogger(LogLevel level) {
        this->logLevel = level;
    }

    RunConfig::RunConfig() : StartupConfig() {
        this->mode = Run;
    }

    std::string StartupConfig::getMongoUri() const {
        return "mongodb://" + this->mongoUser + ":" + this->mongoPass + "@" + this->mongoUrl +
               "/?authSource=admin&authMechanism=SCRAM-SHA-256";
    }

    std::string StartupConfig::getPostgresConnectionString() const {
        return std::string(
            "host=" + pgHost + " port=" + pgPort + " dbname=garraiobide user=" + pgUser + " password=" + pgPass);
    }

    void StartupConfig::setMongo(std::string user, std::string pass, std::string url) {
        this->mongoUser = user;
        this->mongoPass = pass;
        this->mongoUrl = url;
    }

    void StartupConfig::setPostgres(std::string host, std::string port, std::string user, std::string pass) {
        this->pgHost = host;
        this->pgPort = port;
        this->pgUser = user;
        this->pgPass = pass;
    }

    IngestConfig::IngestConfig() : StartupConfig() {
        this->mode = Ingest;
    }

    std::string IngestConfig::getName() const {
        return name;
    }

    std::string IngestConfig::getType() const {
        return type;
    }

    std::string IngestConfig::getUrl() const {
        return url;
    }

    std::string IngestConfig::getCredentials() const {
        return credentials;
    }

    void IngestConfig::setName(std::string name) {
        this->name = name;
    }

    void IngestConfig::setType(std::string type) {
        this->type = type;
    }

    void IngestConfig::setUrl(std::string url) {
        this->url = url;
    }

    void IngestConfig::setCredentials(std::string credentials) {
        this->credentials = credentials;
    }

    StatsConfig::StatsConfig() : StartupConfig() {
        this->mode = Stats;
    }
}
