#include "gtfs_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>

#include "src/core/domain/geo_feature.h"
#include "src/core/domain/geometry.h"
#include "src/core/domain/coordinate.h"
#include "src/core/domain/properties.h"

namespace garraiobide::adapters::ingestion::gtfs {

namespace {

/// Determine stop_type from GTFS fields.
std::string classify_stop(const CsvRow& stop_row) {
    auto lt_it = stop_row.find("location_type");
    if (lt_it != stop_row.end() && lt_it->second == "1") {
        return "parent_station";
    }
    auto ps_it = stop_row.find("parent_station");
    if (ps_it != stop_row.end() && !ps_it->second.empty()) {
        return "child_stop";
    }
    return "standalone";
}

/// Build a stop layer from the GTFS stops data.
core::domain::Layer build_stop_layer(const std::vector<CsvRow>& stops,
                                     const std::string& layer_name) {
    core::domain::Layer layer;
    layer.name = layer_name;
    layer.scale = core::domain::SpatialScale::Urban;

    for (const auto& stop_row : stops) {
        auto stop_id_it = stop_row.find("stop_id");
        auto stop_lat_it = stop_row.find("stop_lat");
        auto stop_lon_it = stop_row.find("stop_lon");

        if (stop_id_it == stop_row.end() ||
            stop_lat_it == stop_row.end() ||
            stop_lon_it == stop_row.end()) {
            continue;
        }

        core::domain::GeoFeature feature;
        feature.id = stop_id_it->second;

        feature.geometry = core::domain::Point{
            core::domain::Coordinate{
                .latitude = std::stod(stop_lat_it->second),
                .longitude = std::stod(stop_lon_it->second)}};

        feature.properties["stop_type"] = classify_stop(stop_row);

        auto ps_it = stop_row.find("parent_station");
        if (ps_it != stop_row.end() && !ps_it->second.empty()) {
            feature.properties["parent_station"] = ps_it->second;
        }

        auto name_it = stop_row.find("stop_name");
        if (name_it != stop_row.end() && !name_it->second.empty()) {
            feature.properties["stop_name"] = name_it->second;
        }

        auto code_it = stop_row.find("stop_code");
        if (code_it != stop_row.end() && !code_it->second.empty()) {
            feature.properties["stop_code"] = code_it->second;
        }

        auto url_it = stop_row.find("stop_url");
        if (url_it != stop_row.end() && !url_it->second.empty()) {
            feature.properties["stop_url"] = url_it->second;
        }

        layer.features.push_back(std::move(feature));
    }

    return layer;
}

/// Build route geometry by selecting the longest trip variant.
core::domain::LineString build_route_geometry(
    const std::string& route_id,
    const std::vector<CsvRow>& trips,
    const std::vector<CsvRow>& stop_times,
    const std::vector<CsvRow>& stops,
    const std::vector<CsvRow>& shapes) {

    std::unordered_map<std::string, const CsvRow*> stop_lookup;
    for (const auto& stop : stops) {
        auto id_it = stop.find("stop_id");
        if (id_it != stop.end()) stop_lookup[id_it->second] = &stop;
    }

    std::vector<const CsvRow*> route_trips;
    for (const auto& trip : trips) {
        auto rid_it = trip.find("route_id");
        if (rid_it != trip.end() && rid_it->second == route_id)
            route_trips.push_back(&trip);
    }

    struct TripCandidate {
        std::string trip_id;
        std::string shape_id;
        std::size_t count{0};
        bool uses_shapes{false};
    };

    std::vector<TripCandidate> candidates;
    for (const auto* trip_row : route_trips) {
        auto trip_id_it = trip_row->find("trip_id");
        if (trip_id_it == trip_row->end()) continue;

        TripCandidate c;
        c.trip_id = trip_id_it->second;

        auto shape_id_it = trip_row->find("shape_id");
        if (shape_id_it != trip_row->end()) c.shape_id = shape_id_it->second;

        if (!c.shape_id.empty()) {
            for (const auto& row : shapes) {
                auto sid = row.find("shape_id");
                if (sid != row.end() && sid->second == c.shape_id) ++c.count;
            }
            if (c.count > 0) { c.uses_shapes = true; candidates.push_back(c); continue; }
        }

        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid != st.end() && tid->second == c.trip_id) ++c.count;
        }
        candidates.push_back(c);
    }

    if (candidates.empty()) return core::domain::LineString{};

    const auto& best = *std::max_element(candidates.begin(), candidates.end(),
        [](const TripCandidate& a, const TripCandidate& b) { return a.count < b.count; });

    core::domain::LineString line;

    if (best.uses_shapes) {
        struct ShapePt { int seq; core::domain::Coordinate coord; };
        std::vector<ShapePt> pts;
        for (const auto& row : shapes) {
            auto sid = row.find("shape_id");
            if (sid == row.end() || sid->second != best.shape_id) continue;
            auto lat = row.find("shape_pt_lat");
            auto lon = row.find("shape_pt_lon");
            auto seq = row.find("shape_pt_sequence");
            if (lat == row.end() || lon == row.end() || seq == row.end()) continue;
            pts.push_back({std::stoi(seq->second),
                {.latitude = std::stod(lat->second), .longitude = std::stod(lon->second)}});
        }
        std::sort(pts.begin(), pts.end(),
            [](const ShapePt& a, const ShapePt& b) { return a.seq < b.seq; });
        for (const auto& p : pts) line.vertices.push_back(p.coord);
    } else {
        struct StEntry { int seq; std::string stop_id; };
        std::vector<StEntry> entries;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid == st.end() || tid->second != best.trip_id) continue;
            auto seq = st.find("stop_sequence");
            auto sid = st.find("stop_id");
            if (seq == st.end() || sid == st.end()) continue;
            entries.push_back({std::stoi(seq->second), sid->second});
        }
        std::sort(entries.begin(), entries.end(),
            [](const StEntry& a, const StEntry& b) { return a.seq < b.seq; });
        for (const auto& e : entries) {
            auto it = stop_lookup.find(e.stop_id);
            if (it == stop_lookup.end()) continue;
            auto lat = it->second->find("stop_lat");
            auto lon = it->second->find("stop_lon");
            if (lat == it->second->end() || lon == it->second->end()) continue;
            line.vertices.push_back(
                {.latitude = std::stod(lat->second), .longitude = std::stod(lon->second)});
        }
    }

    return line;
}

/// Build route layer from GTFS feed data.
core::domain::Layer build_route_layer(const GtfsFeed& feed,
                                      const std::string& layer_name) {
    core::domain::Layer layer;
    layer.name = layer_name;
    layer.scale = core::domain::SpatialScale::Urban;

    for (const auto& route_row : feed.routes) {
        auto route_id_it = route_row.find("route_id");
        if (route_id_it == route_row.end()) continue;
        const std::string& route_id = route_id_it->second;

        core::domain::GeoFeature feature;
        feature.id = route_id;
        feature.geometry = build_route_geometry(
            route_id, feed.trips, feed.stop_times, feed.stops, feed.shapes);

        auto sn = route_row.find("route_short_name");
        if (sn != route_row.end() && !sn->second.empty())
            feature.properties["route_short_name"] = sn->second;

        auto ln = route_row.find("route_long_name");
        if (ln != route_row.end() && !ln->second.empty())
            feature.properties["route_long_name"] = ln->second;

        auto rt = route_row.find("route_type");
        if (rt != route_row.end() && !rt->second.empty())
            feature.properties["route_type"] = static_cast<int64_t>(std::stoll(rt->second));

        auto rc = route_row.find("route_color");
        if (rc != route_row.end() && !rc->second.empty())
            feature.properties["route_color"] = rc->second;

        auto tc = route_row.find("route_text_color");
        if (tc != route_row.end() && !tc->second.empty())
            feature.properties["route_text_color"] = tc->second;

        layer.features.push_back(std::move(feature));
    }

    return layer;
}

}  // namespace

std::string normalize_agency_name(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (c == ' ') {
            result += '_';
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

std::expected<GtfsLayers, core::ports::IngestionError>
parse_gtfs_feed(const GtfsFeed& feed) {
    std::string prefix = "gtfs";
    if (!feed.agency.empty()) {
        auto name_it = feed.agency[0].find("agency_name");
        if (name_it != feed.agency[0].end() && !name_it->second.empty()) {
            prefix = normalize_agency_name(name_it->second);
        }
    }

    GtfsLayers layers;
    layers.stops = build_stop_layer(feed.stops, prefix + "_stops");
    layers.routes = build_route_layer(feed, prefix + "_routes");

    return layers;
}

}  // namespace garraiobide::adapters::ingestion::gtfs
