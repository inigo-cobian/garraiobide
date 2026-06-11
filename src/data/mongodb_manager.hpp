#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>
#include <string>
#include <memory>

namespace data {
    class MongoDBManager {
    public:
        struct Config {
            std::string uri = "mongodb://localhost:27017";
            std::string database = "garraiobide";
            // TODO add max pool, add SSL, auth, etc.
        };

        explicit MongoDBManager(Config cfg);

        ~MongoDBManager() = default;

        MongoDBManager(const MongoDBManager &) = delete;

        MongoDBManager &operator=(const MongoDBManager &) = delete;

        MongoDBManager(MongoDBManager &&) noexcept = default;

        MongoDBManager &operator=(MongoDBManager &&) noexcept = default;

        mongocxx::collection getCollection(std::string_view coll_name) const;

    private:
        Config config_;
        std::unique_ptr<mongocxx::pool> pool_;
        mongocxx::database db_;
    };
}
