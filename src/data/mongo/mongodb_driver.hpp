#pragma once
#include <mongocxx/instance.hpp>

namespace data {
    /**
     * @brief Singleton that holds the MongoDB C++ driver instance.
     *
     * Ensures that the driver is initialized exactly once (the mongocxx::instance
     * must outlive all other MongoDB operations).
     */
    class MongodbDriverInstance {
    public:
        /**
         * @brief Access the single instance.
         * @return MongodbDriverInstance& The singleton reference.
         */
        static MongodbDriverInstance &get() {
            static MongodbDriverInstance instance;
            return instance;
        }

    private:
        MongodbDriverInstance() = default;

        /// The actual driver instance.
        mongocxx::instance instance_;
    };
}
