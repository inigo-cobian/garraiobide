#pragma once

#include <expected>
#include <string>
#include <vector>

#include "csv_parser.h"
#include "gtfs_parser.h"
#include "core/domain/agency.h"
#include "core/domain/route.h"
#include "core/domain/stop.h"
#include "core/ports/data_ingestion_port.h"

namespace garraiobide::adapters::ingestion::gtfs {

/// Result of parsing a GTFS feed into typed domain entities.
struct GtfsEntities {
    std::vector<core::domain::Agency> agencies;
    std::vector<core::domain::Route> routes;
    std::vector<core::domain::Stop> stops;
    /// Maps route_id → ordered list of stop_ids (for building route_stops).
    std::vector<std::pair<std::string, std::vector<std::string>>> route_stop_sequences;
};

/// Parse a GTFS feed into typed domain entities.
/// This is a pure function — all I/O happens before calling this.
/// Produces Agency, Route, and Stop entities with proper relationships.
[[nodiscard]] std::expected<GtfsEntities, core::ports::IngestionError>
parse_gtfs_entities(const GtfsFeed& feed);

}  // namespace garraiobide::adapters::ingestion::gtfs
