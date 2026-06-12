#include "mongodb_manager.hpp"

#include "mongodb_driver.hpp"

namespace data {
    MongoDBManager::MongoDBManager(Config cfg) : config_(std::move(cfg)) {
        MongodbDriverInstance::get();

        mongocxx::uri uri(config_.uri);
        pool_ = std::make_unique<mongocxx::pool>(uri);

        auto client = pool_->acquire();
        if (!client) {
            throw std::runtime_error("Failed to acquire MongoDB client from pool");
        }
        db_ = (*client)[config_.database];
    }

    mongocxx::collection MongoDBManager::getCollection(std::string_view coll_name) const {
        return db_[std::string(coll_name)];
    }
}
