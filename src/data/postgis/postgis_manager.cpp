#include "postgis_manager.hpp"
#include "core/stop.hpp"
#include "core/line.hpp"
#include "core/stop_in_line.hpp"
#include "ui/color.hpp"
#include <stdexcept>
#include <format>

namespace data {
    PostgisManager::PostgisManager(const std::string &connection_string)
        : conn_(connection_string) {
        if (!conn_.is_open())
            throw std::runtime_error("Cannot connect to PostGIS database");
        enablePostGis();
        createTables();
    }

    void PostgisManager::enablePostGis() {
        pqxx::work txn(conn_);
        txn.exec("CREATE EXTENSION IF NOT EXISTS postgis;");
        txn.commit();
    }

    void PostgisManager::createTables() {
        pqxx::work txn(conn_);

        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS stops (
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            geom GEOMETRY(POINT, 4326),
            source TEXT NOT NULL,
            PRIMARY KEY (id, source)
        );
        CREATE INDEX IF NOT EXISTS stops_geom_idx ON stops USING GIST (geom);
    )");

        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS lines (
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            color TEXT,
            text_color TEXT,
            source TEXT NOT NULL,
            PRIMARY KEY (id, source)
        );
    )");

        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS stop_in_line (
            line_id TEXT NOT NULL,
            line_source TEXT NOT NULL,
            stop_id TEXT NOT NULL,
            stop_source TEXT NOT NULL,
            stop_order INTEGER NOT NULL,
            source TEXT,   -- source of the relationship itself (optional)
            PRIMARY KEY (line_id, line_source, stop_id, stop_source),
            FOREIGN KEY (line_id, line_source) REFERENCES lines(id, source) ON DELETE CASCADE,
            FOREIGN KEY (stop_id, stop_source) REFERENCES stops(id, source) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS sil_line_idx ON stop_in_line(line_id, line_source);
        CREATE INDEX IF NOT EXISTS sil_stop_idx ON stop_in_line(stop_id, stop_source);
    )");

        txn.commit();
    }

    void PostgisManager::insertStop(const core::Stop &stop) {
        pqxx::work txn(conn_);
        // Convert lon/lat to PostGIS point (longitude = X, latitude = Y)
        std::string geom_expr = std::format(
            "ST_SetSRID(ST_MakePoint({}, {}), 4326)",
            stop.get_longitude(), stop.get_latitude()
        );
        txn.exec_params(
            "INSERT INTO stops (id, name, geom, source) VALUES ($1, $2, " + geom_expr + ", $3) "
            "ON CONFLICT (id, source) DO UPDATE SET "
            "name = EXCLUDED.name, geom = EXCLUDED.geom, source = EXCLUDED.source;",
            stop.get_id(), stop.get_name(), stop.get_source()
        );
        txn.commit();
    }

    void PostgisManager::insertLine(const core::Line &line) {
        pqxx::work txn(conn_);
        txn.exec_params(
            "INSERT INTO lines (id, name, color, text_color, source) VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (id, source) DO UPDATE SET "
            "name = EXCLUDED.name, color = EXCLUDED.color, "
            "text_color = EXCLUDED.text_color, source = EXCLUDED.source;",
            line.get_id(), line.get_name(),
            ui::toHex(line.get_color()),
            ui::toHex(line.get_text_color()),
            line.get_source()
        );
        txn.commit();
    }

    void PostgisManager::insertStopInLine(const core::StopInLine &sil) {
        pqxx::work txn(conn_);
        std::string source = sil.get_source();
        txn.exec_params(
            "INSERT INTO stop_in_line (line_id, line_source, stop_id, stop_source, stop_order, source) "
            "VALUES ($1, $2, $3, $4, $5, $6) "
            "ON CONFLICT (line_id, line_source, stop_id, stop_source) DO UPDATE SET "
            "stop_order = EXCLUDED.stop_order, source = EXCLUDED.source;",
            sil.get_line_id(), source,
            sil.get_stop_id(), source,
            sil.get_order(), source
        );
        txn.commit();
    }

    bool PostgisManager::stopExists(const std::string &stop_id, const std::string &source) const {
        pqxx::nontransaction txn(conn_);
        auto result = txn.exec_params(
            "SELECT 1 FROM stops WHERE id = $1 AND source = $2 LIMIT 1;",
            stop_id, source
        );
        return !result.empty();
    }

    bool PostgisManager::lineExists(const std::string &line_id, const std::string &source) const {
        pqxx::nontransaction txn(conn_);
        auto result = txn.exec_params(
            "SELECT 1 FROM lines WHERE id = $1 AND source = $2 LIMIT 1;",
            line_id, source
        );
        return !result.empty();
    }
}
