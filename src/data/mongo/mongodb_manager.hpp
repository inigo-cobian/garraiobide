#pragma once

#include <mongocxx/pool.hpp>
#include <string>
#include <memory>

namespace data {
    /**
     * @brief Manages a connection pool to MongoDB and provides access to collections.
     *
     * Configurable with URI, database name, and (in the future) pool size and SSL.
     * Owns a connection pool and exposes the database object.
     */
    class MongoDBManager {
    public:
        struct Config {
            std::string uri = "mongodb://localhost:27017"; ///< Connection URI.
            std::string database = "garraiobide"; ///< Default database name.
            // TODO add max pool, add SSL, auth, etc.
        };

        /**
         * @brief Construct a new MongoDBManager and initialize the pool.
         * @param cfg The connection configuration.
         */
        explicit MongoDBManager(Config cfg);

        ~MongoDBManager() = default;

        MongoDBManager(const MongoDBManager &) = delete;

        MongoDBManager &operator=(const MongoDBManager &) = delete;

        MongoDBManager(MongoDBManager &&) noexcept = default;

        MongoDBManager &operator=(MongoDBManager &&) noexcept = default;

        /**
         * @brief Obtain a collection from the configured database.
         * @param collection_name Name of the collection.
         * @return mongocxx::collection A collection object (obtained from the pool).
         */
        [[nodiscard]] mongocxx::collection getCollection(std::string_view collection_name) const;

    private:
        Config config_;
        std::unique_ptr<mongocxx::pool> pool_;
        mongocxx::database db_;
    };
}
