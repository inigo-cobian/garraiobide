#include "config.hpp"

#include "log_level.hpp"

namespace core {
    LaunchMode StartupConfig::getMode() {
        return mode;
    }

    LogLevel StartupConfig::getLogLevel() {
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

    std::string StartupConfig::getPostgresUri() const {
        // TODO manage credentials (?)
        return this->pgUrl;
    }

    void StartupConfig::setMongo(std::string user, std::string pass, std::string url) {
        this->mongoUser = user;
        this->mongoPass = pass;
        this->mongoUrl = url;
    }

    void StartupConfig::setPostgres(std::string user, std::string pass, std::string url) {
        this->pgUser = user;
        this->pgPass = pass;
        this->pgUrl = url;
    }

    IngestConfig::IngestConfig() : StartupConfig() {
        this->mode = Ingest;
    }

    std::string IngestConfig::getName() {
        return name;
    }

    std::string IngestConfig::getType() {
        return type;
    }

    std::string IngestConfig::getUrl() {
        return url;
    }

    std::string IngestConfig::getCredentials() {
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
