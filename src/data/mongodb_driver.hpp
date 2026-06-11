#pragma once
#include <mongocxx/instance.hpp>

namespace data {
    class MongodbDriverInstance {
    public:
        static MongodbDriverInstance &get() {
            static MongodbDriverInstance instance;
            return instance;
        }

    private:
        MongodbDriverInstance() = default;

        mongocxx::instance instance_;
    };
}
