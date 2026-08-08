#pragma once

#include <memory>
#include <string>

#include <pqxx/pqxx>

#include "../../core/ports/transit_repository_port.h"

namespace garraiobide::adapters::persistence {

/// PostGIS-backed implementation of TransitRepositoryPort.
/// Uses libpqxx for parameterized queries against a PostgreSQL/PostGIS database.
/// Geometry is stored/retrieved via ST_GeomFromGeoJSON / ST_AsGeoJSON.
class PostgisTransitRepository final : public core::ports::TransitRepositoryPort {
   public:
    /// Construct with a PostgreSQL connection string.
    /// Example: "host=localhost port=5432 dbname=garraiobide user=postgres password=secret"
    explicit PostgisTransitRepository(const std::string& connection_string);

    ~PostgisTransitRepository() override = default;

    // ── Agency operations ─────────────────────────────────────────────

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    save_agency(const core::domain::Agency& agency) override;

    [[nodiscard]] std::expected<core::domain::Agency, core::ports::TransitRepositoryError>
    find_agency(const std::string& id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Agency>, core::ports::TransitRepositoryError>
    list_agencies() override;

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    remove_agency(const std::string& id) override;

    // ── Route operations ──────────────────────────────────────────────

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    save_route(const core::domain::Route& route) override;

    [[nodiscard]] std::expected<core::domain::Route, core::ports::TransitRepositoryError>
    find_route(const std::string& id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Route>, core::ports::TransitRepositoryError>
    list_routes(const std::string& agency_id = "") override;

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    remove_route(const std::string& id) override;

    // ── Stop operations ───────────────────────────────────────────────

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    save_stop(const core::domain::Stop& stop) override;

    [[nodiscard]] std::expected<core::domain::Stop, core::ports::TransitRepositoryError>
    find_stop(const std::string& id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Stop>, core::ports::TransitRepositoryError>
    list_stops(const std::string& stop_type_filter = "") override;

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    remove_stop(const std::string& id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Stop>, core::ports::TransitRepositoryError>
    find_children_of(const std::string& parent_stop_id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Stop>, core::ports::TransitRepositoryError>
    query_stops(const core::domain::BoundingBox& extent) override;

    // ── Entrance operations ───────────────────────────────────────────

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    save_entrance(const core::domain::Entrance& entrance) override;

    [[nodiscard]] std::expected<core::domain::Entrance, core::ports::TransitRepositoryError>
    find_entrance(const std::string& id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Entrance>, core::ports::TransitRepositoryError>
    list_entrances(const std::string& stop_id) override;

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    remove_entrance(const std::string& id) override;

    // ── Relational queries ────────────────────────────────────────────

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    add_route_stop(const std::string& route_id, const std::string& stop_id,
                   int stop_sequence) override;

    [[nodiscard]] std::expected<void, core::ports::TransitRepositoryError>
    remove_route_stop(const std::string& route_id, const std::string& stop_id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Stop>, core::ports::TransitRepositoryError>
    find_stops_for_route(const std::string& route_id) override;

    [[nodiscard]] std::expected<std::vector<core::domain::Route>, core::ports::TransitRepositoryError>
    find_routes_for_stop(const std::string& stop_id) override;

   private:
    std::unique_ptr<pqxx::connection> conn_;

    /// Convert a domain Geometry to a GeoJSON string for ST_GeomFromGeoJSON.
    [[nodiscard]] static std::string geometry_to_geojson(const core::domain::Geometry& geom);

    /// Parse a GeoJSON string (from ST_AsGeoJSON) back into a domain Geometry.
    [[nodiscard]] static core::domain::Geometry geojson_to_geometry(const std::string& geojson);

    /// Parse station_sequence JSONB into StationEntry vector.
    [[nodiscard]] static std::vector<core::domain::StationEntry>
    parse_station_sequence(const std::string& json_str);

    /// Serialize StationEntry vector to JSON string for JSONB storage.
    [[nodiscard]] static std::string
    serialize_station_sequence(const std::vector<core::domain::StationEntry>& entries);

    /// Map stop_type string from DB to StopType enum.
    [[nodiscard]] static core::domain::StopType string_to_stop_type(const std::string& s);

    /// Map StopType enum to DB string.
    [[nodiscard]] static std::string stop_type_to_string(core::domain::StopType type);

    /// Build a Stop from a result row (expects columns in a specific order).
    [[nodiscard]] static core::domain::Stop row_to_stop(const pqxx::row& row);

    /// Build a Route from a result row.
    [[nodiscard]] static core::domain::Route row_to_route(const pqxx::row& row);
};

}  // namespace garraiobide::adapters::persistence
