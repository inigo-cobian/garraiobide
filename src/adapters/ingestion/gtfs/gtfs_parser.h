#pragma once

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../adapters/ingestion/gtfs/csv_parser.h"
#include "../../core/domain/layer.h"
#include "../../core/ports/data_ingestion_port.h"

namespace garraiobide::adapters::ingestion::gtfs {

/// Parsed contents of a GTFS feed (already extracted from ZIP).
struct GtfsFeed {
    std::vector<CsvRow> agency;
    std::vector<CsvRow> routes;
    std::vector<CsvRow> trips;
    std::vector<CsvRow> stops;
    std::vector<CsvRow> stop_times;
    std::vector<CsvRow> shapes;  // May be empty if shapes.txt absent
};

/// Result of a successful GTFS parse: a route layer and a stop layer.
struct GtfsLayers {
    core::domain::Layer routes;
    core::domain::Layer stops;
};

/// Parses GTFS CSV data into domain Layer objects.
/// This is a pure function — all I/O happens before calling this.
[[nodiscard]] std::expected<GtfsLayers, core::ports::IngestionError>
parse_gtfs_feed(const GtfsFeed& feed);

/// Normalize an agency name to a layer name prefix:
/// lowercase, spaces replaced by underscores.
[[nodiscard]] std::string normalize_agency_name(const std::string& name);

}  // namespace garraiobide::adapters::ingestion::gtfs
