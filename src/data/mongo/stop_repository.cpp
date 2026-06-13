#include "stop_repository.hpp"

#include <mongocxx/collection.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>
#include <bsoncxx/types.hpp>

namespace data {
    StopRepository::StopRepository(MongoDBManager &db_manager) : db_manager_(db_manager) {
    }

    bsoncxx::document::value StopRepository::to_bson(const gtfs::Stop &stop) {
        using bsoncxx::builder::basic::document;
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::sub_array;

        document doc{};

        doc.append(kvp("_id", stop.get_id()));
        doc.append(kvp("name", stop.get_name()));
        doc.append(kvp("locationType", static_cast<int>(stop.location_type())));

        const auto &point = stop.get_point();
        auto coords = bsoncxx::builder::basic::array{};
        coords.append(point.getX()); // longitude
        coords.append(point.getY()); // latitude

        auto location = document{};
        location.append(kvp("type", "Point"));
        location.append(kvp("coordinates", coords));
        doc.append(kvp("location", location));

        if (stop.parent_station().has_value()) {
            doc.append(kvp("parentStation", *stop.parent_station()));
        } else {
            doc.append(kvp("parentStation", bsoncxx::types::b_null{}));
        }

        return doc.extract();
    }

    gtfs::Stop StopRepository::from_bson(const bsoncxx::document::view &doc) {
        std::string id = bsoncxx::string::to_string(doc["_id"].get_string().value);
        std::string name = bsoncxx::string::to_string(doc["name"].get_string().value);

        auto location = doc["location"];
        auto coords = location["coordinates"].get_array().value;
        auto lon = coords[0].get_double();
        auto lat = coords[1].get_double();

        int type_int = doc["locationType"].get_int32();
        auto type = static_cast<gtfs::LocationType>(type_int);

        std::optional<std::string> parent;
        auto parent_elem = doc["parentStation"];
        if (parent_elem.type() != bsoncxx::type::k_null) {
            parent = bsoncxx::string::to_string(parent_elem.get_string().value);
        }

        return gtfs::Stop(std::move(id), std::move(name), lat,
                          lon, type, parent);
    }

    void StopRepository::insert(const gtfs::Stop &stop) const {
        auto collection = db_manager_.getCollection(kCollectionName);
        auto doc = to_bson(stop);
        collection.insert_one(doc.view());
    }

    std::vector<std::string> StopRepository::insert_many(const std::vector<gtfs::Stop> &stops) const {
        if (stops.empty()) {
            return {};
        }
        auto collection = db_manager_.getCollection(kCollectionName);

        std::vector<bsoncxx::document::value> documents;
        documents.reserve(stops.size());

        for (const auto &stop: stops) {
            documents.push_back(to_bson(stop));
        }

        std::vector<bsoncxx::document::view> views;
        views.reserve(documents.size());
        for (const auto &doc: documents) {
            views.push_back(doc.view());
        }

        try {
            auto result = collection.insert_many(views);

            std::vector<std::string> inserted_ids;
            inserted_ids.reserve(result->inserted_count());

            for (const auto &id_pair: result->inserted_ids()) {
                const auto &id_element = id_pair.second;
                std::string id_str = bsoncxx::string::to_string(id_element.get_string().value);
                inserted_ids.push_back(std::move(id_str));
            }

            return inserted_ids;
        } catch (const mongocxx::exception &e) {
            // TODO Handle duplicate key errors or other bulk write issues
            throw std::runtime_error(std::string("Bulk insert failed: ") + e.what());
        }
    }

    std::optional<gtfs::Stop> StopRepository::find_by_id(const std::string &id) const {
        auto collection = db_manager_.getCollection(kCollectionName);
        using bsoncxx::builder::basic::make_document;
        using bsoncxx::builder::basic::kvp;
        auto filter = make_document(kvp("_id", id));
        auto result = collection.find_one(filter.view());
        if (result) {
            return from_bson(result->view());
        }
        return std::nullopt;
    }

    std::vector<gtfs::Stop> StopRepository::find_all() const {
        auto collection = db_manager_.getCollection(kCollectionName);
        std::vector<gtfs::Stop> stops;
        auto cursor = collection.find({});
        for (auto &&doc: cursor) {
            stops.push_back(from_bson(doc));
        }
        return stops;
    }

    void StopRepository::update(const gtfs::Stop &stop) const {
        auto collection = db_manager_.getCollection(kCollectionName);
        using bsoncxx::builder::basic::make_document;
        using bsoncxx::builder::basic::kvp;
        auto filter = make_document(kvp("_id", stop.get_id()));
        auto update = make_document(kvp("$set", to_bson(stop).view()));
        auto result = collection.update_one(filter.view(), update.view());
        if (!result || result->modified_count() == 0) {
            throw std::runtime_error("Stop not found or unchanged: " + stop.get_id());
        }
    }

    void StopRepository::remove(const std::string &id) const {
        auto collection = db_manager_.getCollection(kCollectionName);
        using bsoncxx::builder::basic::make_document;
        using bsoncxx::builder::basic::kvp;
        auto filter = make_document(kvp("_id", id));
        auto result = collection.delete_one(filter.view());
        if (!result || result->deleted_count() == 0) {
            throw std::runtime_error("Stop not found: " + id);
        }
    }
}
