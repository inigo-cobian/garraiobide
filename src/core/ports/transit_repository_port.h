#pragma once

#include <expected>
#include <string>
#include <vector>

#include "../domain/agency.h"
#include "../domain/bounding_box.h"
#include "../domain/entrance.h"
#include "../domain/route.h"
#include "../domain/stop.h"

namespace garraiobide::core::ports {

/// Errors that transit repository adapters may report.
enum class TransitRepositoryError {
    NotFound,
    WriteError,
    ConnectionError,
    DuplicateEntity,
    ForeignKeyViolation,
};

/// Driven port: typed CRUD operations on transit entities.
///
/// This port operates on strongly-typed domain entities (Agency, Route, Stop,
/// Entrance) rather than generic GeoFeature/Layer containers. Adapters back
/// this port with a relational store (PostGIS) that enforces referential
/// integrity between entities.
class TransitRepositoryPort {
   public:
    virtual ~TransitRepositoryPort() = default;

    // ── Agency operations ─────────────────────────────────────────────

    /// Persist an agency. Fails with DuplicateEntity if id already exists.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    save_agency(const domain::Agency& agency) = 0;

    /// Find an agency by its id.
    [[nodiscard]] virtual std::expected<domain::Agency, TransitRepositoryError>
    find_agency(const std::string& id) = 0;

    /// List all agency ids.
    [[nodiscard]] virtual std::expected<std::vector<domain::Agency>, TransitRepositoryError>
    list_agencies() = 0;

    /// Remove an agency by id. Cascades to its routes.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    remove_agency(const std::string& id) = 0;

    // ── Route operations ──────────────────────────────────────────────

    /// Persist a route. Fails with ForeignKeyViolation if agency_id is invalid.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    save_route(const domain::Route& route) = 0;

    /// Find a route by its id.
    [[nodiscard]] virtual std::expected<domain::Route, TransitRepositoryError>
    find_route(const std::string& id) = 0;

    /// List all routes, optionally filtered by agency.
    [[nodiscard]] virtual std::expected<std::vector<domain::Route>, TransitRepositoryError>
    list_routes(const std::string& agency_id = "") = 0;

    /// Remove a route by id. Cascades to route_stops.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    remove_route(const std::string& id) = 0;

    // ── Stop operations ───────────────────────────────────────────────

    /// Persist a stop.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    save_stop(const domain::Stop& stop) = 0;

    /// Find a stop by its id.
    [[nodiscard]] virtual std::expected<domain::Stop, TransitRepositoryError>
    find_stop(const std::string& id) = 0;

    /// List all stops, optionally filtered by stop_type.
    [[nodiscard]] virtual std::expected<std::vector<domain::Stop>, TransitRepositoryError>
    list_stops(const std::string& stop_type_filter = "") = 0;

    /// Remove a stop by id. Cascades to entrances and route_stops.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    remove_stop(const std::string& id) = 0;

    /// Find all child stops of a parent station.
    [[nodiscard]] virtual std::expected<std::vector<domain::Stop>, TransitRepositoryError>
    find_children_of(const std::string& parent_stop_id) = 0;

    /// Query stops within a bounding box (spatial query).
    [[nodiscard]] virtual std::expected<std::vector<domain::Stop>, TransitRepositoryError>
    query_stops(const domain::BoundingBox& extent) = 0;

    // ── Entrance operations ───────────────────────────────────────────

    /// Persist an entrance.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    save_entrance(const domain::Entrance& entrance) = 0;

    /// Find an entrance by its id.
    [[nodiscard]] virtual std::expected<domain::Entrance, TransitRepositoryError>
    find_entrance(const std::string& id) = 0;

    /// List all entrances for a given stop.
    [[nodiscard]] virtual std::expected<std::vector<domain::Entrance>, TransitRepositoryError>
    list_entrances(const std::string& stop_id) = 0;

    /// Remove an entrance by id.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    remove_entrance(const std::string& id) = 0;

    // ── Relational queries ────────────────────────────────────────────

    /// Link a stop to a route at a given sequence position.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    add_route_stop(const std::string& route_id, const std::string& stop_id,
                   int stop_sequence) = 0;

    /// Remove a stop from a route.
    [[nodiscard]] virtual std::expected<void, TransitRepositoryError>
    remove_route_stop(const std::string& route_id, const std::string& stop_id) = 0;

    /// Get all stops served by a route, ordered by stop_sequence.
    [[nodiscard]] virtual std::expected<std::vector<domain::Stop>, TransitRepositoryError>
    find_stops_for_route(const std::string& route_id) = 0;

    /// Get all routes serving a stop.
    [[nodiscard]] virtual std::expected<std::vector<domain::Route>, TransitRepositoryError>
    find_routes_for_stop(const std::string& stop_id) = 0;
};

}  // namespace garraiobide::core::ports
