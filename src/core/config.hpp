#pragma once
#include "log_level.hpp"
#include <string>

namespace core {
    /**
     * @brief Launch modes supported by garraiobide.
     */
    enum LaunchMode {
        Run, ///< "Normal" execution mode.
        Ingest, ///< Data ingestion mode.
        Stats ///< Display statistics mode.
    };

    /**
     * @brief Base class for all startup configurations.
     *
     * Holds common settings: launch mode, log level, and database credentials
     * for MongoDB and Postgres. Provides getters and setters for these.
     */
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

        /// @return The selected launch mode.
        [[nodiscard]] LaunchMode getMode() const;

        /// @return The configured log level.
        [[nodiscard]] LogLevel getLogLevel() const;

        /**
         * @brief Initializes the global logger with the given severity level.
         * @param level Minimum log level to output.
         */
        void initializeLogger(LogLevel level);

        /**
         * @brief Builds a MongoDB connection URI from the stored credentials.
         * @return A URI string of the form "mongodb://user:pass@host".
         */
        [[nodiscard]] std::string getMongoUri() const;

        /**
         * @brief Builds a Postgres connection string from the stored credentials.
         * @return A libpq‑style connection string.
         */
        [[nodiscard]] std::string getPostgresConnectionString() const;

        /**
         * @brief Set MongoDB credentials.
         * @param user Username.
         * @param pass Password.
         * @param url Host URL (e.g. "localhost:27017").
         */
        void setMongo(std::string user, std::string pass, std::string url);

        /**
         * @brief Set Postgres credentials.
         * @param host Database host.
         * @param port Port number.
         * @param user Username.
         * @param pass Password.
         */
        void setPostgres(std::string host, std::string port, std::string user, std::string pass);
    };

    /**
     * @brief Configuration for the normal/run mode.
     */
    class RunConfig : public StartupConfig {
        // empty
    public:
        RunConfig();

        ~RunConfig() = default;
    };

    /**
      * @brief Configuration for the ingest mode.
      *
      * Adds parameters needed to download and import a resource:
      * a name, a feed type, a URL, and optional credentials.
      */
    class IngestConfig : public StartupConfig {
        std::string name;
        std::string type;
        std::string url;
        std::string credentials; // TODO credentials should be managed according to the credential needs
    public:
        IngestConfig();

        ~IngestConfig() = default;

        /// @return The human‑readable and unique name of the feed.
        [[nodiscard]] std::string getName() const;

        /// @return The type of the feed (e.g. "gtfs").
        [[nodiscard]] std::string getType() const;

        /// @return The URL from which to download the feed.
        [[nodiscard]] std::string getUrl() const;

        /// @return The credentials (if any) for accessing the URL.
        [[nodiscard]] std::string getCredentials() const;

        void setName(std::string name);

        void setType(std::string type);

        void setUrl(std::string url);

        void setCredentials(std::string credentials);
    };

    /**
     * @brief Configuration for the statistics mode.
     */
    class StatsConfig : public StartupConfig {
        // empty
    public:
        StatsConfig();

        ~StatsConfig() = default;
    };
}
