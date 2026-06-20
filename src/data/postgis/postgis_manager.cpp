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

        // Stops table with PostGIS geometry column
        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS stops (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            geom GEOMETRY(POINT, 4326),
            source TEXT
        );
        CREATE INDEX IF NOT EXISTS stops_geom_idx ON stops USING GIST (geom);
    )");

        // Lines table (no geometry needed)
        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS lines (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            color TEXT,
            text_color TEXT,
            source TEXT
        );
    )");

        // Stop‑in‑line table (relationship)
        txn.exec(R"(
        CREATE TABLE IF NOT EXISTS stop_in_line (
            line_id TEXT NOT NULL,
            stop_id TEXT NOT NULL,
            stop_order INTEGER NOT NULL,
            source TEXT,
            PRIMARY KEY (line_id, stop_id),
            FOREIGN KEY (line_id) REFERENCES lines(id) ON DELETE CASCADE,
            FOREIGN KEY (stop_id) REFERENCES stops(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS sil_line_idx ON stop_in_line(line_id);
        CREATE INDEX IF NOT EXISTS sil_stop_idx ON stop_in_line(stop_id);
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
            "ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, geom = EXCLUDED.geom, source = EXCLUDED.source;",
            stop.get_id(), stop.get_name(), stop.get_source()
        );
        txn.commit();
    }

    void PostgisManager::insertLine(const core::Line &line) {
        pqxx::work txn(conn_);
        txn.exec_params(
            "INSERT INTO lines (id, name, color, text_color, source) VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, color = EXCLUDED.color, "
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
        txn.exec_params(
            "INSERT INTO stop_in_line (line_id, stop_id, stop_order, source) VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (line_id, stop_id) DO UPDATE SET stop_order = EXCLUDED.stop_order, source = EXCLUDED.source;",
            sil.get_line_id(), sil.get_stop_id(), sil.get_order(), sil.get_source()
        );
        txn.commit();
    }

    bool PostgisManager::stopExists(const std::string &stop_id) const {
        pqxx::nontransaction txn(conn_);
        auto result = txn.exec_params("SELECT 1 FROM stops WHERE id = $1 LIMIT 1;", stop_id);
        return !result.empty();
    }

    bool PostgisManager::lineExists(const std::string &line_id) const {
        pqxx::nontransaction txn(conn_);
        auto result = txn.exec_params("SELECT 1 FROM lines WHERE id = $1 LIMIT 1;", line_id);
        return !result.empty();
    }
}
