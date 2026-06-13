#pragma once

#include <optional>
#include <string>
#include <vector>

#include "gtfs/stop.hpp"
#include "mongodb_manager.hpp"

namespace data {
    class StopRepository {
        MongoDBManager &db_manager_;

    public:
        explicit StopRepository(MongoDBManager &db_manager);

        void insert(const gtfs::Stop &stop) const;

        std::vector<std::string> insert_many(const std::vector<gtfs::Stop>&) const;

        std::optional<gtfs::Stop> find_by_id(const std::string &id) const;

        std::vector<gtfs::Stop> find_all() const;

        void update(const gtfs::Stop &stop) const;

        void remove(const std::string &id) const;

    private:
        static constexpr std::string_view kCollectionName = "stops";

        static bsoncxx::document::value to_bson(const gtfs::Stop &stop);

        static gtfs::Stop from_bson(const bsoncxx::document::view &doc);
    };
} // namespace data
