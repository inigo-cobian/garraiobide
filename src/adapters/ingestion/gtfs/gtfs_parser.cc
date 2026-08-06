#include "gtfs_parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

#include <nlohmann/json.hpp>

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

/// Build a map of stop_id → set of route_ids by joining stop_times and trips.
/// Then merge parent↔child relationships so that parents and children share
/// the union of all route memberships across the station group.
std::unordered_map<std::string, std::unordered_set<std::string>>
build_stop_route_map(const std::vector<CsvRow>& stops,
                     const std::vector<CsvRow>& stop_times,
                     const std::vector<CsvRow>& trips) {
    std::unordered_map<std::string, std::unordered_set<std::string>> stop_route_map;

    // Index trips by trip_id → route_id
    std::unordered_map<std::string, std::string> trip_route_index;
    for (const auto& trip : trips) {
        auto tid = trip.find("trip_id");
        auto rid = trip.find("route_id");
        if (tid != trip.end() && rid != trip.end()) {
            trip_route_index[tid->second] = rid->second;
        }
    }

    // Join stop_times → trips to populate stop_route_map
    for (const auto& st : stop_times) {
        auto sid = st.find("stop_id");
        auto tid = st.find("trip_id");
        if (sid == st.end() || tid == st.end()) continue;
        auto it = trip_route_index.find(tid->second);
        if (it != trip_route_index.end()) {
            stop_route_map[sid->second].insert(it->second);
        }
    }

    // Build parent_id → children and child → parent relationships
    std::unordered_map<std::string, std::vector<std::string>> parent_children;
    for (const auto& stop : stops) {
        auto sid = stop.find("stop_id");
        auto ps = stop.find("parent_station");
        if (sid == stop.end()) continue;
        if (ps != stop.end() && !ps->second.empty()) {
            parent_children[ps->second].push_back(sid->second);
        }
    }

    // Merge: parent gets all children's routes, then children get merged parent routes
    for (const auto& [parent_id, children] : parent_children) {
        auto& parent_routes = stop_route_map[parent_id];
        for (const auto& child_id : children) {
            auto& child_routes = stop_route_map[child_id];
            // Parent absorbs child routes
            parent_routes.insert(child_routes.begin(), child_routes.end());
        }
        // Children absorb merged parent routes
        for (const auto& child_id : children) {
            stop_route_map[child_id].insert(parent_routes.begin(), parent_routes.end());
        }
    }

    return stop_route_map;
}

/// Build a stop layer from the GTFS feed data (stops, stop_times, trips).
core::domain::Layer build_stop_layer(const std::vector<CsvRow>& stops,
                                     const std::vector<CsvRow>& stop_times,
                                     const std::vector<CsvRow>& trips,
                                     const std::string& layer_name) {
    core::domain::Layer layer;
    layer.name = layer_name;
    layer.scale = core::domain::SpatialScale::Urban;

    // Compute route membership for each stop
    auto stop_route_map = build_stop_route_map(stops, stop_times, trips);

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

        // Serialize route_ids as a JSON array string
        nlohmann::json route_ids_arr = nlohmann::json::array();
        auto route_it = stop_route_map.find(stop_id_it->second);
        if (route_it != stop_route_map.end()) {
            for (const auto& r : route_it->second) {
                route_ids_arr.push_back(r);
            }
        }
        feature.properties["route_ids"] = route_ids_arr.dump();

        layer.features.push_back(std::move(feature));
    }

    return layer;
}

/// Extract a sorted LineString from a given shape_id.
core::domain::LineString extract_shape_line(
    const std::string& shape_id,
    const std::vector<CsvRow>& shapes) {

    struct ShapePt { int seq; core::domain::Coordinate coord; };
    std::vector<ShapePt> pts;
    for (const auto& row : shapes) {
        auto sid = row.find("shape_id");
        if (sid == row.end() || sid->second != shape_id) continue;
        auto lat = row.find("shape_pt_lat");
        auto lon = row.find("shape_pt_lon");
        auto seq = row.find("shape_pt_sequence");
        if (lat == row.end() || lon == row.end() || seq == row.end()) continue;
        pts.push_back({std::stoi(seq->second),
            {.latitude = std::stod(lat->second), .longitude = std::stod(lon->second)}});
    }
    std::sort(pts.begin(), pts.end(),
        [](const ShapePt& a, const ShapePt& b) { return a.seq < b.seq; });

    core::domain::LineString line;
    for (const auto& p : pts) line.vertices.push_back(p.coord);
    return line;
}

/// Build route geometry by combining all unique shapes across trips.
/// Returns a LineString if a single shape, MultiLineString if multiple,
/// or falls back to stop coordinates if no shapes are available.
core::domain::Geometry build_route_geometry(
    const std::string& route_id,
    const std::vector<CsvRow>& trips,
    const std::vector<CsvRow>& stop_times,
    const std::vector<CsvRow>& stops,
    const std::vector<CsvRow>& shapes) {

    // Collect all unique shape_ids referenced by trips of this route.
    std::vector<std::string> unique_shape_ids;
    for (const auto& trip : trips) {
        auto rid_it = trip.find("route_id");
        if (rid_it == trip.end() || rid_it->second != route_id) continue;

        auto shape_id_it = trip.find("shape_id");
        if (shape_id_it == trip.end() || shape_id_it->second.empty()) continue;

        const std::string& sid = shape_id_it->second;
        if (std::find(unique_shape_ids.begin(), unique_shape_ids.end(), sid)
            == unique_shape_ids.end()) {
            unique_shape_ids.push_back(sid);
        }
    }

    // If we have shape data, build geometry from all unique shapes.
    if (!unique_shape_ids.empty()) {
        std::vector<core::domain::LineString> lines;
        for (const auto& sid : unique_shape_ids) {
            auto line = extract_shape_line(sid, shapes);
            if (!line.vertices.empty()) lines.push_back(std::move(line));
        }

        if (lines.size() == 1) {
            return lines[0];
        }
        if (lines.size() > 1) {
            core::domain::MultiLineString multi;
            for (auto& l : lines) {
                multi.lines.push_back(std::move(l.vertices));
            }
            return multi;
        }
    }

    // Fallback: build geometry from stop coordinates of the longest trip.
    std::unordered_map<std::string, const CsvRow*> stop_lookup;
    for (const auto& stop : stops) {
        auto id_it = stop.find("stop_id");
        if (id_it != stop.end()) stop_lookup[id_it->second] = &stop;
    }

    std::string best_trip_id;
    std::size_t best_count = 0;
    for (const auto& trip : trips) {
        auto rid_it = trip.find("route_id");
        if (rid_it == trip.end() || rid_it->second != route_id) continue;
        auto tid_it = trip.find("trip_id");
        if (tid_it == trip.end()) continue;

        std::size_t count = 0;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid != st.end() && tid->second == tid_it->second) ++count;
        }
        if (count > best_count) {
            best_count = count;
            best_trip_id = tid_it->second;
        }
    }

    core::domain::LineString line;
    if (!best_trip_id.empty()) {
        struct StEntry { int seq; std::string stop_id; };
        std::vector<StEntry> entries;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid == st.end() || tid->second != best_trip_id) continue;
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

/// Build per-route ordered station sequence.
/// For each route, picks the longest trip and extracts parent stations ordered
/// by stop_sequence. Returns a map of route_id → JSON array string where each
/// entry is {"name": <station_name>, "child_count": <n>}.
std::unordered_map<std::string, std::string>
build_station_sequences(const std::vector<CsvRow>& stops,
                        const std::vector<CsvRow>& stop_times,
                        const std::vector<CsvRow>& trips) {
    // Index trips by trip_id → route_id
    std::unordered_map<std::string, std::string> trip_route_index;
    for (const auto& trip : trips) {
        auto tid = trip.find("trip_id");
        auto rid = trip.find("route_id");
        if (tid != trip.end() && rid != trip.end()) {
            trip_route_index[tid->second] = rid->second;
        }
    }

    // Group stop_times by trip_id, tracking count per trip
    std::unordered_map<std::string, std::size_t> trip_stop_count;
    for (const auto& st : stop_times) {
        auto tid = st.find("trip_id");
        if (tid != st.end()) {
            trip_stop_count[tid->second]++;
        }
    }

    // For each route, find the trip with the most stops (the "canonical" trip)
    std::unordered_map<std::string, std::string> route_best_trip;  // route_id → trip_id
    std::unordered_map<std::string, std::size_t> route_best_count;
    for (const auto& [trip_id, count] : trip_stop_count) {
        auto rit = trip_route_index.find(trip_id);
        if (rit == trip_route_index.end()) continue;
        const std::string& route_id = rit->second;
        if (count > route_best_count[route_id]) {
            route_best_count[route_id] = count;
            route_best_trip[route_id] = trip_id;
        }
    }

    // Build stop lookup: stop_id → row
    std::unordered_map<std::string, const CsvRow*> stop_lookup;
    for (const auto& stop : stops) {
        auto id_it = stop.find("stop_id");
        if (id_it != stop.end()) stop_lookup[id_it->second] = &stop;
    }

    // Build parent_station → child count
    std::unordered_map<std::string, int> parent_child_count;
    for (const auto& stop : stops) {
        auto ps = stop.find("parent_station");
        if (ps != stop.end() && !ps->second.empty()) {
            parent_child_count[ps->second]++;
        }
    }
    // Standalone stops with same stop_id as a parent should count children of that parent
    // Actually: count children for parent_station stops and standalone stops that have
    // a corresponding est_ parent. Let's count based on the parent_station field.

    // For each route's best trip, extract ordered parent stations
    std::unordered_map<std::string, std::string> result;

    for (const auto& [route_id, trip_id] : route_best_trip) {
        // Collect stop_times for this trip, ordered by stop_sequence
        struct StEntry { int seq; std::string stop_id; };
        std::vector<StEntry> entries;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid == st.end() || tid->second != trip_id) continue;
            auto seq = st.find("stop_sequence");
            auto sid = st.find("stop_id");
            if (seq == st.end() || sid == st.end()) continue;
            entries.push_back({std::stoi(seq->second), sid->second});
        }
        std::sort(entries.begin(), entries.end(),
            [](const StEntry& a, const StEntry& b) { return a.seq < b.seq; });

        // For each stop in the trip, resolve to its parent station
        // and deduplicate consecutive parents
        nlohmann::json sequence_arr = nlohmann::json::array();
        std::string last_parent_id;

        for (const auto& entry : entries) {
            auto stop_it = stop_lookup.find(entry.stop_id);
            if (stop_it == stop_lookup.end()) continue;
            const CsvRow& stop_row = *stop_it->second;

            // Determine the parent station id
            std::string stop_id;
            std::string parent_id;
            std::string station_name;

            auto lt_it = stop_row.find("parent_station");
            bool is_parent_or_standalone = lt_it != stop_row.end() && lt_it->second.empty();

            // TODO change, can be parent or standalone
            if (is_parent_or_standalone) {
                stop_id = parent_id = entry.stop_id;
            } else {
                auto ps_it = stop_row.find("parent_station");
                if (ps_it != stop_row.end() && !ps_it->second.empty()) {
                    parent_id = ps_it->second;
                }
            }

            // Skip if value is empty
            if (stop_id.empty()) continue;

            // Skip if same parent as previous (avoid duplicates)
            if (parent_id == last_parent_id) continue;
            last_parent_id = parent_id;

            // Get the parent station name
            auto parent_it = stop_lookup.find(parent_id);
            if (parent_it != stop_lookup.end()) {
                auto name_it = parent_it->second->find("stop_name");
                if (name_it != parent_it->second->end()) {
                    station_name = name_it->second;
                }
            }
            if (station_name.empty()) {
                // Fallback: use the stop's own name
                auto name_it = stop_row.find("stop_name");
                if (name_it != stop_row.end()) {
                    station_name = name_it->second;
                }
            }

            // Count children of this parent station
            int child_count = 0;
            auto cc_it = parent_child_count.find(parent_id);
            if (cc_it != parent_child_count.end()) {
                child_count = cc_it->second;
            }

            nlohmann::json station_obj;
            station_obj["id"] = stop_id;
            station_obj["name"] = station_name;
            station_obj["child_count"] = child_count;
            sequence_arr.push_back(station_obj);
        }

        result[route_id] = sequence_arr.dump();
    }

    return result;
}

/// Build route layer from GTFS feed data.
core::domain::Layer build_route_layer(const GtfsFeed& feed,
                                      const std::string& layer_name) {
    core::domain::Layer layer;
    layer.name = layer_name;
    layer.scale = core::domain::SpatialScale::Urban;

    // Build per-route station sequences
    auto sequences = build_station_sequences(feed.stops, feed.stop_times, feed.trips);

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

        // Attach station sequence for this route
        auto seq_it = sequences.find(route_id);
        if (seq_it != sequences.end()) {
            feature.properties["station_sequence"] = seq_it->second;
        }

        layer.features.push_back(std::move(feature));
    }

    return layer;
}

}  // namespace

std::string normalize_agency_name(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
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
    layers.stops = build_stop_layer(feed.stops, feed.stop_times, feed.trips, prefix + "_stops");
    layers.routes = build_route_layer(feed, prefix + "_routes");

    return layers;
}

}  // namespace garraiobide::adapters::ingestion::gtfs
