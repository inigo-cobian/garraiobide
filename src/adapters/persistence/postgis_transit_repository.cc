#include "postgis_transit_repository.h"

#include <stdexcept>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace garraiobide::adapters::persistence {

using core::domain::Agency;
using core::domain::BoundingBox;
using core::domain::Coordinate;
using core::domain::Entrance;
using core::domain::Geometry;
using core::domain::LineString;
using core::domain::MultiLineString;
using core::domain::Point;
using core::domain::Polygon;
using core::domain::Route;
using core::domain::StationEntry;
using core::domain::Stop;
using core::domain::StopType;
using core::ports::TransitRepositoryError;
using Error = TransitRepositoryError;

// ── Constructor ───────────────────────────────────────────────────────────

PostgisTransitRepository::PostgisTransitRepository(const std::string& connection_string)
    : conn_(std::make_unique<pqxx::connection>(connection_string)) {
    if (!conn_->is_open()) {
        throw std::runtime_error("Failed to open PostGIS connection");
    }
}

// ── Geometry serialization helpers ────────────────────────────────────────

std::string PostgisTransitRepository::geometry_to_geojson(const Geometry& geom) {
    nlohmann::json j;
    std::visit([&j](const auto& g) {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Point>) {
            j["type"] = "Point";
            j["coordinates"] = {g.position.longitude, g.position.latitude};
        } else if constexpr (std::is_same_v<T, LineString>) {
            j["type"] = "LineString";
            auto& coords = j["coordinates"] = nlohmann::json::array();
            for (const auto& v : g.vertices) {
                coords.push_back({v.longitude, v.latitude});
            }
        } else if constexpr (std::is_same_v<T, MultiLineString>) {
            j["type"] = "MultiLineString";
            auto& coords = j["coordinates"] = nlohmann::json::array();
            for (const auto& line : g.lines) {
                auto line_arr = nlohmann::json::array();
                for (const auto& v : line) {
                    line_arr.push_back({v.longitude, v.latitude});
                }
                coords.push_back(line_arr);
            }
        } else if constexpr (std::is_same_v<T, Polygon>) {
            j["type"] = "Polygon";
            auto& coords = j["coordinates"] = nlohmann::json::array();
            for (const auto& ring : g.rings) {
                auto ring_arr = nlohmann::json::array();
                for (const auto& v : ring) {
                    ring_arr.push_back({v.longitude, v.latitude});
                }
                coords.push_back(ring_arr);
            }
        }
    }, geom);
    return j.dump();
}

Geometry PostgisTransitRepository::geojson_to_geometry(const std::string& geojson) {
    auto j = nlohmann::json::parse(geojson);
    std::string type = j["type"];

    if (type == "Point") {
        auto& c = j["coordinates"];
        return Point{.position = {.latitude = c[1].get<double>(),
                                  .longitude = c[0].get<double>()}};
    }
    if (type == "LineString") {
        LineString ls;
        for (const auto& c : j["coordinates"]) {
            ls.vertices.push_back({.latitude = c[1].get<double>(),
                                   .longitude = c[0].get<double>()});
        }
        return ls;
    }
    if (type == "MultiLineString") {
        MultiLineString mls;
        for (const auto& line : j["coordinates"]) {
            std::vector<Coordinate> coords;
            for (const auto& c : line) {
                coords.push_back({.latitude = c[1].get<double>(),
                                  .longitude = c[0].get<double>()});
            }
            mls.lines.push_back(std::move(coords));
        }
        return mls;
    }
    // Polygon
    Polygon poly;
    for (const auto& ring : j["coordinates"]) {
        std::vector<Coordinate> coords;
        for (const auto& c : ring) {
            coords.push_back({.latitude = c[1].get<double>(),
                              .longitude = c[0].get<double>()});
        }
        poly.rings.push_back(std::move(coords));
    }
    return poly;
}

// ── Station sequence helpers ──────────────────────────────────────────────

std::vector<StationEntry>
PostgisTransitRepository::parse_station_sequence(const std::string& json_str) {
    std::vector<StationEntry> entries;
    if (json_str.empty() || json_str == "null") return entries;
    auto arr = nlohmann::json::parse(json_str);
    if (!arr.is_array()) return entries;
    for (const auto& obj : arr) {
        entries.push_back({
            .id = obj.value("id", ""),
            .name = obj.value("name", ""),
            .child_count = obj.value("child_count", 0),
        });
    }
    return entries;
}

std::string PostgisTransitRepository::serialize_station_sequence(
    const std::vector<StationEntry>& entries) {
    if (entries.empty()) return "null";
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries) {
        arr.push_back({{"id", e.id}, {"name", e.name}, {"child_count", e.child_count}});
    }
    return arr.dump();
}

// ── StopType helpers ──────────────────────────────────────────────────────

StopType PostgisTransitRepository::string_to_stop_type(const std::string& s) {
    if (s == "parent_station") return StopType::ParentStation;
    if (s == "child_stop") return StopType::ChildStop;
    return StopType::Standalone;
}

std::string PostgisTransitRepository::stop_type_to_string(StopType type) {
    switch (type) {
        case StopType::ParentStation: return "parent_station";
        case StopType::ChildStop: return "child_stop";
        case StopType::Standalone: return "standalone";
    }
    return "standalone";
}

// ── Row parsing helpers ───────────────────────────────────────────────────

Stop PostgisTransitRepository::row_to_stop(const pqxx::row& row) {
    Stop stop;
    stop.id = row["id"].as<std::string>();
    stop.name = row["name"].as<std::string>();
    if (!row["code"].is_null()) stop.code = row["code"].as<std::string>();
    if (!row["url"].is_null()) stop.url = row["url"].as<std::string>();

    // Geometry comes as GeoJSON from ST_AsGeoJSON
    auto geojson = row["geojson"].as<std::string>();
    auto geom = geojson_to_geometry(geojson);
    if (std::holds_alternative<Point>(geom)) {
        stop.position = std::get<Point>(geom).position;
    }

    stop.stop_type = string_to_stop_type(row["stop_type"].as<std::string>());
    if (!row["parent_stop_id"].is_null()) {
        stop.parent_stop_id = row["parent_stop_id"].as<std::string>();
    }
    return stop;
}

Route PostgisTransitRepository::row_to_route(const pqxx::row& row) {
    Route route;
    route.id = row["id"].as<std::string>();
    route.agency_id = row["agency_id"].as<std::string>();
    if (!row["short_name"].is_null()) route.short_name = row["short_name"].as<std::string>();
    if (!row["long_name"].is_null()) route.long_name = row["long_name"].as<std::string>();
    route.route_type = row["route_type"].as<int>();
    if (!row["color"].is_null()) route.color = row["color"].as<std::string>();
    if (!row["text_color"].is_null()) route.text_color = row["text_color"].as<std::string>();
    if (!row["geojson"].is_null()) {
        route.geometry = geojson_to_geometry(row["geojson"].as<std::string>());
    }
    if (!row["station_sequence"].is_null()) {
        route.station_sequence = parse_station_sequence(
            row["station_sequence"].as<std::string>());
    }
    return route;
}

// ── Agency CRUD ───────────────────────────────────────────────────────────

std::expected<void, Error>
PostgisTransitRepository::save_agency(const Agency& agency) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO agencies (id, name, url, timezone, lang, phone) "
            "VALUES ($1, $2, $3, $4, $5, $6)",
            agency.id, agency.name,
            agency.url ? std::optional<std::string>{*agency.url} : std::nullopt,
            agency.timezone ? std::optional<std::string>{*agency.timezone} : std::nullopt,
            agency.lang ? std::optional<std::string>{*agency.lang} : std::nullopt,
            agency.phone ? std::optional<std::string>{*agency.phone} : std::nullopt);
        txn.commit();
        return {};
    } catch (const pqxx::unique_violation&) {
        return std::unexpected(Error::DuplicateEntity);
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::WriteError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<Agency, Error>
PostgisTransitRepository::find_agency(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, name, url, timezone, lang, phone FROM agencies WHERE id = $1",
            id);
        txn.commit();

        if (result.empty()) return std::unexpected(Error::NotFound);

        const auto& row = result[0];
        Agency agency;
        agency.id = row["id"].as<std::string>();
        agency.name = row["name"].as<std::string>();
        if (!row["url"].is_null()) agency.url = row["url"].as<std::string>();
        if (!row["timezone"].is_null()) agency.timezone = row["timezone"].as<std::string>();
        if (!row["lang"].is_null()) agency.lang = row["lang"].as<std::string>();
        if (!row["phone"].is_null()) agency.phone = row["phone"].as<std::string>();
        return agency;
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::ConnectionError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Agency>, Error>
PostgisTransitRepository::list_agencies() {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec("SELECT id, name, url, timezone, lang, phone FROM agencies ORDER BY id");
        txn.commit();

        std::vector<Agency> agencies;
        agencies.reserve(result.size());
        for (const auto& row : result) {
            Agency agency;
            agency.id = row["id"].as<std::string>();
            agency.name = row["name"].as<std::string>();
            if (!row["url"].is_null()) agency.url = row["url"].as<std::string>();
            if (!row["timezone"].is_null()) agency.timezone = row["timezone"].as<std::string>();
            if (!row["lang"].is_null()) agency.lang = row["lang"].as<std::string>();
            if (!row["phone"].is_null()) agency.phone = row["phone"].as<std::string>();
            agencies.push_back(std::move(agency));
        }
        return agencies;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<void, Error>
PostgisTransitRepository::remove_agency(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params("DELETE FROM agencies WHERE id = $1", id);
        txn.commit();
        if (result.affected_rows() == 0) return std::unexpected(Error::NotFound);
        return {};
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

// ── Route CRUD ────────────────────────────────────────────────────────────

std::expected<void, Error>
PostgisTransitRepository::save_route(const Route& route) {
    try {
        pqxx::work txn(*conn_);

        std::optional<std::string> geom_json;
        if (route.geometry.has_value()) {
            geom_json = geometry_to_geojson(route.geometry.value());
        }

        std::optional<std::string> seq_json;
        if (!route.station_sequence.empty()) {
            seq_json = serialize_station_sequence(route.station_sequence);
        }

        if (geom_json.has_value()) {
            txn.exec_params(
                "INSERT INTO routes (id, agency_id, short_name, long_name, route_type, "
                "color, text_color, geometry, station_sequence) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, "
                "ST_SetSRID(ST_GeomFromGeoJSON($8), 4326), $9::jsonb)",
                route.id, route.agency_id,
                route.short_name ? std::optional<std::string>{*route.short_name} : std::nullopt,
                route.long_name ? std::optional<std::string>{*route.long_name} : std::nullopt,
                route.route_type,
                route.color ? std::optional<std::string>{*route.color} : std::nullopt,
                route.text_color ? std::optional<std::string>{*route.text_color} : std::nullopt,
                *geom_json,
                seq_json ? std::optional<std::string>{*seq_json} : std::nullopt);
        } else {
            txn.exec_params(
                "INSERT INTO routes (id, agency_id, short_name, long_name, route_type, "
                "color, text_color, station_sequence) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8::jsonb)",
                route.id, route.agency_id,
                route.short_name ? std::optional<std::string>{*route.short_name} : std::nullopt,
                route.long_name ? std::optional<std::string>{*route.long_name} : std::nullopt,
                route.route_type,
                route.color ? std::optional<std::string>{*route.color} : std::nullopt,
                route.text_color ? std::optional<std::string>{*route.text_color} : std::nullopt,
                seq_json ? std::optional<std::string>{*seq_json} : std::nullopt);
        }

        txn.commit();
        return {};
    } catch (const pqxx::unique_violation&) {
        return std::unexpected(Error::DuplicateEntity);
    } catch (const pqxx::foreign_key_violation&) {
        return std::unexpected(Error::ForeignKeyViolation);
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::WriteError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<Route, Error>
PostgisTransitRepository::find_route(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, agency_id, short_name, long_name, route_type, color, text_color, "
            "ST_AsGeoJSON(geometry) AS geojson, station_sequence::text "
            "FROM routes WHERE id = $1", id);
        txn.commit();

        if (result.empty()) return std::unexpected(Error::NotFound);
        return row_to_route(result[0]);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Route>, Error>
PostgisTransitRepository::list_routes(const std::string& agency_id) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result result;

        if (agency_id.empty()) {
            result = txn.exec(
                "SELECT id, agency_id, short_name, long_name, route_type, color, text_color, "
                "ST_AsGeoJSON(geometry) AS geojson, station_sequence::text "
                "FROM routes ORDER BY id");
        } else {
            result = txn.exec_params(
                "SELECT id, agency_id, short_name, long_name, route_type, color, text_color, "
                "ST_AsGeoJSON(geometry) AS geojson, station_sequence::text "
                "FROM routes WHERE agency_id = $1 ORDER BY id", agency_id);
        }
        txn.commit();

        std::vector<Route> routes;
        routes.reserve(result.size());
        for (const auto& row : result) {
            routes.push_back(row_to_route(row));
        }
        return routes;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<void, Error>
PostgisTransitRepository::remove_route(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params("DELETE FROM routes WHERE id = $1", id);
        txn.commit();
        if (result.affected_rows() == 0) return std::unexpected(Error::NotFound);
        return {};
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

// ── Stop CRUD ─────────────────────────────────────────────────────────────

std::expected<void, Error>
PostgisTransitRepository::save_stop(const Stop& stop) {
    try {
        pqxx::work txn(*conn_);
        auto point_geojson = geometry_to_geojson(Point{.position = stop.position});

        txn.exec_params(
            "INSERT INTO stops (id, name, code, url, geometry, stop_type, parent_stop_id) "
            "VALUES ($1, $2, $3, $4, ST_SetSRID(ST_GeomFromGeoJSON($5), 4326), $6, $7)",
            stop.id, stop.name,
            stop.code ? std::optional<std::string>{*stop.code} : std::nullopt,
            stop.url ? std::optional<std::string>{*stop.url} : std::nullopt,
            point_geojson,
            stop_type_to_string(stop.stop_type),
            stop.parent_stop_id ? std::optional<std::string>{*stop.parent_stop_id} : std::nullopt);
        txn.commit();
        return {};
    } catch (const pqxx::unique_violation&) {
        return std::unexpected(Error::DuplicateEntity);
    } catch (const pqxx::foreign_key_violation&) {
        return std::unexpected(Error::ForeignKeyViolation);
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::WriteError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<Stop, Error>
PostgisTransitRepository::find_stop(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, name, code, url, ST_AsGeoJSON(geometry) AS geojson, "
            "stop_type, parent_stop_id FROM stops WHERE id = $1", id);
        txn.commit();

        if (result.empty()) return std::unexpected(Error::NotFound);
        return row_to_stop(result[0]);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Stop>, Error>
PostgisTransitRepository::list_stops(const std::string& stop_type_filter) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result result;

        if (stop_type_filter.empty()) {
            result = txn.exec(
                "SELECT id, name, code, url, ST_AsGeoJSON(geometry) AS geojson, "
                "stop_type, parent_stop_id FROM stops ORDER BY id");
        } else {
            result = txn.exec_params(
                "SELECT id, name, code, url, ST_AsGeoJSON(geometry) AS geojson, "
                "stop_type, parent_stop_id FROM stops "
                "WHERE stop_type = $1 ORDER BY id", stop_type_filter);
        }
        txn.commit();

        std::vector<Stop> stops;
        stops.reserve(result.size());
        for (const auto& row : result) {
            stops.push_back(row_to_stop(row));
        }
        return stops;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<void, Error>
PostgisTransitRepository::remove_stop(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params("DELETE FROM stops WHERE id = $1", id);
        txn.commit();
        if (result.affected_rows() == 0) return std::unexpected(Error::NotFound);
        return {};
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Stop>, Error>
PostgisTransitRepository::find_children_of(const std::string& parent_stop_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, name, code, url, ST_AsGeoJSON(geometry) AS geojson, "
            "stop_type, parent_stop_id FROM stops "
            "WHERE parent_stop_id = $1 ORDER BY id", parent_stop_id);
        txn.commit();

        std::vector<Stop> stops;
        stops.reserve(result.size());
        for (const auto& row : result) {
            stops.push_back(row_to_stop(row));
        }
        return stops;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Stop>, Error>
PostgisTransitRepository::query_stops(const BoundingBox& extent) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, name, code, url, ST_AsGeoJSON(geometry) AS geojson, "
            "stop_type, parent_stop_id FROM stops "
            "WHERE ST_Intersects(geometry, ST_MakeEnvelope($1, $2, $3, $4, 4326)) "
            "ORDER BY id",
            extent.south_west.longitude, extent.south_west.latitude,
            extent.north_east.longitude, extent.north_east.latitude);
        txn.commit();

        std::vector<Stop> stops;
        stops.reserve(result.size());
        for (const auto& row : result) {
            stops.push_back(row_to_stop(row));
        }
        return stops;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

// ── Entrance CRUD ─────────────────────────────────────────────────────────

std::expected<void, Error>
PostgisTransitRepository::save_entrance(const Entrance& entrance) {
    try {
        pqxx::work txn(*conn_);
        auto point_geojson = geometry_to_geojson(Point{.position = entrance.position});

        txn.exec_params(
            "INSERT INTO entrances (id, stop_id, name, geometry) "
            "VALUES ($1, $2, $3, ST_SetSRID(ST_GeomFromGeoJSON($4), 4326))",
            entrance.id, entrance.stop_id,
            entrance.name ? std::optional<std::string>{*entrance.name} : std::nullopt,
            point_geojson);
        txn.commit();
        return {};
    } catch (const pqxx::unique_violation&) {
        return std::unexpected(Error::DuplicateEntity);
    } catch (const pqxx::foreign_key_violation&) {
        return std::unexpected(Error::ForeignKeyViolation);
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::WriteError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<Entrance, Error>
PostgisTransitRepository::find_entrance(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, stop_id, name, ST_AsGeoJSON(geometry) AS geojson "
            "FROM entrances WHERE id = $1", id);
        txn.commit();

        if (result.empty()) return std::unexpected(Error::NotFound);

        const auto& row = result[0];
        Entrance entrance;
        entrance.id = row["id"].as<std::string>();
        entrance.stop_id = row["stop_id"].as<std::string>();
        if (!row["name"].is_null()) entrance.name = row["name"].as<std::string>();

        auto geom = geojson_to_geometry(row["geojson"].as<std::string>());
        if (std::holds_alternative<Point>(geom)) {
            entrance.position = std::get<Point>(geom).position;
        }
        return entrance;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Entrance>, Error>
PostgisTransitRepository::list_entrances(const std::string& stop_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT id, stop_id, name, ST_AsGeoJSON(geometry) AS geojson "
            "FROM entrances WHERE stop_id = $1 ORDER BY id", stop_id);
        txn.commit();

        std::vector<Entrance> entrances;
        entrances.reserve(result.size());
        for (const auto& row : result) {
            Entrance entrance;
            entrance.id = row["id"].as<std::string>();
            entrance.stop_id = row["stop_id"].as<std::string>();
            if (!row["name"].is_null()) entrance.name = row["name"].as<std::string>();
            auto geom = geojson_to_geometry(row["geojson"].as<std::string>());
            if (std::holds_alternative<Point>(geom)) {
                entrance.position = std::get<Point>(geom).position;
            }
            entrances.push_back(std::move(entrance));
        }
        return entrances;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<void, Error>
PostgisTransitRepository::remove_entrance(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params("DELETE FROM entrances WHERE id = $1", id);
        txn.commit();
        if (result.affected_rows() == 0) return std::unexpected(Error::NotFound);
        return {};
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

// ── Route-Stop relationship operations ────────────────────────────────────

std::expected<void, Error>
PostgisTransitRepository::add_route_stop(const std::string& route_id,
                                         const std::string& stop_id,
                                         int stop_sequence) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO route_stops (route_id, stop_id, stop_sequence) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (route_id, stop_id) DO UPDATE SET stop_sequence = $3",
            route_id, stop_id, stop_sequence);
        txn.commit();
        return {};
    } catch (const pqxx::foreign_key_violation&) {
        return std::unexpected(Error::ForeignKeyViolation);
    } catch (const pqxx::sql_error&) {
        return std::unexpected(Error::WriteError);
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<void, Error>
PostgisTransitRepository::remove_route_stop(const std::string& route_id,
                                            const std::string& stop_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "DELETE FROM route_stops WHERE route_id = $1 AND stop_id = $2",
            route_id, stop_id);
        txn.commit();
        if (result.affected_rows() == 0) return std::unexpected(Error::NotFound);
        return {};
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Stop>, Error>
PostgisTransitRepository::find_stops_for_route(const std::string& route_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT s.id, s.name, s.code, s.url, "
            "ST_AsGeoJSON(s.geometry) AS geojson, "
            "s.stop_type, s.parent_stop_id "
            "FROM stops s "
            "INNER JOIN route_stops rs ON rs.stop_id = s.id "
            "WHERE rs.route_id = $1 "
            "ORDER BY rs.stop_sequence", route_id);
        txn.commit();

        std::vector<Stop> stops;
        stops.reserve(result.size());
        for (const auto& row : result) {
            stops.push_back(row_to_stop(row));
        }
        return stops;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

std::expected<std::vector<Route>, Error>
PostgisTransitRepository::find_routes_for_stop(const std::string& stop_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT r.id, r.agency_id, r.short_name, r.long_name, r.route_type, "
            "r.color, r.text_color, ST_AsGeoJSON(r.geometry) AS geojson, "
            "r.station_sequence::text "
            "FROM routes r "
            "INNER JOIN route_stops rs ON rs.route_id = r.id "
            "WHERE rs.stop_id = $1 "
            "ORDER BY r.id", stop_id);
        txn.commit();

        std::vector<Route> routes;
        routes.reserve(result.size());
        for (const auto& row : result) {
            routes.push_back(row_to_route(row));
        }
        return routes;
    } catch (...) {
        return std::unexpected(Error::ConnectionError);
    }
}

}  // namespace garraiobide::adapters::persistence
