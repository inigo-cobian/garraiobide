#include "gtfs_ingestion_adapter.h"

#include <zip.h>

#include <string>
#include <vector>

#include "csv_parser.h"
#include "gtfs_parser.h"

namespace garraiobide::adapters::ingestion::gtfs {

std::expected<std::vector<core::domain::GeoFeature>, core::ports::IngestionError>
GtfsIngestionAdapter::load_features(const std::string& source) {
    // Read required CSV files from the ZIP archive
    auto agency_csv = read_zip_entry(source, "agency.txt");
    if (!agency_csv) {
        return std::unexpected(agency_csv.error());
    }

    auto routes_csv = read_zip_entry(source, "routes.txt");
    if (!routes_csv) {
        return std::unexpected(routes_csv.error());
    }

    auto trips_csv = read_zip_entry(source, "trips.txt");
    if (!trips_csv) {
        return std::unexpected(trips_csv.error());
    }

    auto stops_csv = read_zip_entry(source, "stops.txt");
    if (!stops_csv) {
        return std::unexpected(stops_csv.error());
    }

    auto stop_times_csv = read_zip_entry(source, "stop_times.txt");
    if (!stop_times_csv) {
        return std::unexpected(stop_times_csv.error());
    }

    // Read optional shapes.txt
    std::string shapes_csv_content;
    if (zip_contains(source, "shapes.txt")) {
        auto shapes_csv = read_zip_entry(source, "shapes.txt");
        if (shapes_csv) {
            shapes_csv_content = std::move(*shapes_csv);
        }
    }

    // Parse each CSV string into row maps
    GtfsFeed feed;
    feed.agency = parse_csv(*agency_csv);
    feed.routes = parse_csv(*routes_csv);
    feed.trips = parse_csv(*trips_csv);
    feed.stops = parse_csv(*stops_csv);
    feed.stop_times = parse_csv(*stop_times_csv);
    feed.shapes = parse_csv(shapes_csv_content);

    // Parse the GTFS feed into domain layers
    auto layers = parse_gtfs_feed(feed);
    if (!layers) {
        return std::unexpected(layers.error());
    }

    // Flatten route + stop features into a single vector
    std::vector<core::domain::GeoFeature> all_features;
    all_features.reserve(layers->routes.features.size() + layers->stops.features.size());

    for (auto& feature : layers->routes.features) {
        all_features.push_back(std::move(feature));
    }
    for (auto& feature : layers->stops.features) {
        all_features.push_back(std::move(feature));
    }

    return all_features;
}

std::expected<std::vector<core::domain::GeoFeature>, core::ports::IngestionError>
GtfsIngestionAdapter::load_features_within(const std::string& source,
                                           const core::domain::BoundingBox& extent) {
    // TODO: Implement with bounding box filtering
    return std::unexpected(core::ports::IngestionError::SourceNotFound);
}

std::expected<std::string, core::ports::IngestionError>
GtfsIngestionAdapter::read_zip_entry(const std::string& zip_path,
                                     const std::string& entry_name) {
    int errcode = 0;
    zip_t* archive = zip_open(zip_path.c_str(), ZIP_RDONLY, &errcode);
    if (archive == nullptr) {
        return std::unexpected(core::ports::IngestionError::SourceNotFound);
    }

    zip_int64_t index = zip_name_locate(archive, entry_name.c_str(), 0);
    if (index < 0) {
        zip_close(archive);
        return std::unexpected(core::ports::IngestionError::ParseError);
    }

    zip_stat_t stat;
    if (zip_stat_index(archive, static_cast<zip_uint64_t>(index), 0, &stat) != 0) {
        zip_close(archive);
        return std::unexpected(core::ports::IngestionError::ParseError);
    }

    zip_file_t* file = zip_fopen_index(archive, static_cast<zip_uint64_t>(index), 0);
    if (file == nullptr) {
        zip_close(archive);
        return std::unexpected(core::ports::IngestionError::ParseError);
    }

    std::string contents(stat.size, '\0');
    zip_int64_t bytes_read = zip_fread(file, contents.data(), stat.size);
    zip_fclose(file);
    zip_close(archive);

    if (bytes_read < 0 || static_cast<zip_uint64_t>(bytes_read) != stat.size) {
        return std::unexpected(core::ports::IngestionError::ParseError);
    }

    return contents;
}

bool GtfsIngestionAdapter::zip_contains(const std::string& zip_path,
                                        const std::string& entry_name) {
    int errcode = 0;
    zip_t* archive = zip_open(zip_path.c_str(), ZIP_RDONLY, &errcode);
    if (archive == nullptr) {
        return false;
    }

    zip_int64_t index = zip_name_locate(archive, entry_name.c_str(), 0);
    zip_close(archive);
    return index >= 0;
}

}  // namespace garraiobide::adapters::ingestion::gtfs
