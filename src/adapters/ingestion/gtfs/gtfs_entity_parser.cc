#include "gtfs_entity_parser.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "core/domain/geometry.h"

namespace garraiobide::adapters::ingestion::gtfs {

namespace {

using core::domain::Agency;
using core::domain::Coordinate;
using core::domain::LineString;
using core::domain::MultiLineString;
using core::domain::Route;
using core::domain::StationEntry;
using core::domain::Stop;
using core::domain::StopType;

/// Classify a GTFS stop row into our StopType enum.
StopType classify_stop_type(const CsvRow& row) {
    auto lt_it = row.find("location_type");
    if (lt_it != row.end() && lt_it->second == "1") {
        return StopType::ParentStation;
    }
    auto ps_it = row.find("parent_station");
    if (ps_it != row.end() && !ps_it->second.empty()) {
        return StopType::ChildStop;
    }
    return StopType::Standalone;
}

/// Build stop_id → set<route_id> map by joining stop_times and trips.
/// Propagates routes to parent/child station groups.
std::unordered_map<std::string, std::unordered_set<std::string>>
build_stop_routes(const std::vector<CsvRow>& stops,
                  const std::vector<CsvRow>& stop_times,
                  const std::vector<CsvRow>& trips) {
    std::unordered_map<std::string, std::unordered_set<std::string>> map;

    // trip_id → route_id index
    std::unordered_map<std::string, std::string> trip_route;
    for (const auto& trip : trips) {
        auto tid = trip.find("trip_id");
        auto rid = trip.find("route_id");
        if (tid != trip.end() && rid != trip.end()) {
            trip_route[tid->second] = rid->second;
        }
    }

    // stop_times → stop_route_map
    for (const auto& st : stop_times) {
        auto sid = st.find("stop_id");
        auto tid = st.find("trip_id");
        if (sid == st.end() || tid == st.end()) continue;
        auto it = trip_route.find(tid->second);
        if (it != trip_route.end()) {
            map[sid->second].insert(it->second);
        }
    }

    // Parent ↔ child propagation
    std::unordered_map<std::string, std::vector<std::string>> parent_children;
    for (const auto& stop : stops) {
        auto sid = stop.find("stop_id");
        auto ps = stop.find("parent_station");
        if (sid == stop.end()) continue;
        if (ps != stop.end() && !ps->second.empty()) {
            parent_children[ps->second].push_back(sid->second);
        }
    }
    for (const auto& [parent_id, children] : parent_children) {
        auto& parent_routes = map[parent_id];
        for (const auto& child_id : children) {
            parent_routes.insert(map[child_id].begin(), map[child_id].end());
        }
        for (const auto& child_id : children) {
            map[child_id].insert(parent_routes.begin(), parent_routes.end());
        }
    }

    return map;
}

/// Extract a sorted LineString from shapes for a given shape_id.
LineString extract_shape(const std::string& shape_id,
                         const std::vector<CsvRow>& shapes) {
    struct Pt { int seq; Coordinate coord; };
    std::vector<Pt> pts;
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
        [](const Pt& a, const Pt& b) { return a.seq < b.seq; });

    LineString line;
    for (const auto& p : pts) line.vertices.push_back(p.coord);
    return line;
}

/// Build route geometry from shapes/stop fallback (same logic as existing parser).
core::domain::Geometry build_route_geom(
    const std::string& route_id,
    const std::vector<CsvRow>& trips,
    const std::vector<CsvRow>& stop_times,
    const std::vector<CsvRow>& stops,
    const std::vector<CsvRow>& shapes) {

    // Collect unique shape_ids for trips of this route
    std::vector<std::string> shape_ids;
    for (const auto& trip : trips) {
        auto rid = trip.find("route_id");
        if (rid == trip.end() || rid->second != route_id) continue;
        auto sid = trip.find("shape_id");
        if (sid == trip.end() || sid->second.empty()) continue;
        if (std::find(shape_ids.begin(), shape_ids.end(), sid->second) == shape_ids.end()) {
            shape_ids.push_back(sid->second);
        }
    }

    if (!shape_ids.empty()) {
        std::vector<LineString> lines;
        for (const auto& sid : shape_ids) {
            auto line = extract_shape(sid, shapes);
            if (!line.vertices.empty()) lines.push_back(std::move(line));
        }
        if (lines.size() == 1) return lines[0];
        if (lines.size() > 1) {
            MultiLineString multi;
            for (auto& l : lines) multi.lines.push_back(std::move(l.vertices));
            return multi;
        }
    }

    // Fallback: stop coordinates from longest trip
    std::unordered_map<std::string, const CsvRow*> stop_lookup;
    for (const auto& stop : stops) {
        auto id_it = stop.find("stop_id");
        if (id_it != stop.end()) stop_lookup[id_it->second] = &stop;
    }

    std::string best_trip;
    std::size_t best_count = 0;
    for (const auto& trip : trips) {
        auto rid = trip.find("route_id");
        if (rid == trip.end() || rid->second != route_id) continue;
        auto tid = trip.find("trip_id");
        if (tid == trip.end()) continue;
        std::size_t count = 0;
        for (const auto& st : stop_times) {
            auto t = st.find("trip_id");
            if (t != st.end() && t->second == tid->second) ++count;
        }
        if (count > best_count) { best_count = count; best_trip = tid->second; }
    }

    LineString line;
    if (!best_trip.empty()) {
        struct Entry { int seq; std::string stop_id; };
        std::vector<Entry> entries;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid == st.end() || tid->second != best_trip) continue;
            auto seq = st.find("stop_sequence");
            auto sid = st.find("stop_id");
            if (seq == st.end() || sid == st.end()) continue;
            entries.push_back({std::stoi(seq->second), sid->second});
        }
        std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.seq < b.seq; });
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

/// Build per-route ordered stop sequence (stop_ids in order).
/// Returns route_id → vector of stop_ids.
std::vector<std::pair<std::string, std::vector<std::string>>>
build_route_stop_sequences(const std::vector<CsvRow>& stops,
                           const std::vector<CsvRow>& stop_times,
                           const std::vector<CsvRow>& trips) {
    // trip_id → route_id
    std::unordered_map<std::string, std::string> trip_route;
    for (const auto& trip : trips) {
        auto tid = trip.find("trip_id");
        auto rid = trip.find("route_id");
        if (tid != trip.end() && rid != trip.end())
            trip_route[tid->second] = rid->second;
    }

    // Find longest trip per route
    std::unordered_map<std::string, std::size_t> trip_stop_count;
    for (const auto& st : stop_times) {
        auto tid = st.find("trip_id");
        if (tid != st.end()) trip_stop_count[tid->second]++;
    }

    std::unordered_map<std::string, std::string> route_best_trip;
    std::unordered_map<std::string, std::size_t> route_best_count;
    for (const auto& [trip_id, count] : trip_stop_count) {
        auto rit = trip_route.find(trip_id);
        if (rit == trip_route.end()) continue;
        if (count > route_best_count[rit->second]) {
            route_best_count[rit->second] = count;
            route_best_trip[rit->second] = trip_id;
        }
    }

    // stop_id → parent_station
    std::unordered_map<std::string, std::string> stop_parent;
    for (const auto& stop : stops) {
        auto sid = stop.find("stop_id");
        auto ps = stop.find("parent_station");
        if (sid != stop.end() && ps != stop.end() && !ps->second.empty()) {
            stop_parent[sid->second] = ps->second;
        }
    }

    std::vector<std::pair<std::string, std::vector<std::string>>> result;

    for (const auto& [route_id, trip_id] : route_best_trip) {
        struct Entry { int seq; std::string stop_id; };
        std::vector<Entry> entries;
        for (const auto& st : stop_times) {
            auto tid = st.find("trip_id");
            if (tid == st.end() || tid->second != trip_id) continue;
            auto seq = st.find("stop_sequence");
            auto sid = st.find("stop_id");
            if (seq == st.end() || sid == st.end()) continue;
            entries.push_back({std::stoi(seq->second), sid->second});
        }
        std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.seq < b.seq; });

        // Resolve to parent stations and deduplicate consecutive
        std::vector<std::string> stop_ids;
        std::string last_resolved;
        for (const auto& e : entries) {
            std::string resolved = e.stop_id;
            auto pit = stop_parent.find(e.stop_id);
            if (pit != stop_parent.end()) resolved = pit->second;
            if (resolved != last_resolved) {
                stop_ids.push_back(resolved);
                last_resolved = resolved;
            }
        }
        result.emplace_back(route_id, std::move(stop_ids));
    }

    return result;
}

/// Build station sequence (StationEntry vector) for a route given stop_ids.
std::vector<StationEntry> build_station_entries(
    const std::vector<std::string>& stop_ids,
    const std::unordered_map<std::string, const CsvRow*>& stop_lookup,
    const std::unordered_map<std::string, int>& parent_child_count) {

    std::vector<StationEntry> entries;
    for (const auto& sid : stop_ids) {
        StationEntry entry;
        entry.id = sid;
        auto it = stop_lookup.find(sid);
        if (it != stop_lookup.end()) {
            auto name_it = it->second->find("stop_name");
            if (name_it != it->second->end()) entry.name = name_it->second;
        }
        auto cc_it = parent_child_count.find(sid);
        if (cc_it != parent_child_count.end()) entry.child_count = cc_it->second;
        entries.push_back(std::move(entry));
    }
    return entries;
}

}  // namespace

// ── Public function ───────────────────────────────────────────────────────

std::expected<GtfsEntities, core::ports::IngestionError>
parse_gtfs_entities(const GtfsFeed& feed) {
    GtfsEntities result;

    // ── Parse agencies ────────────────────────────────────────────────
    std::string default_agency_id;
    for (const auto& row : feed.agency) {
        Agency agency;
        auto id_it = row.find("agency_id");
        if (id_it != row.end() && !id_it->second.empty()) {
            agency.id = id_it->second;
        } else {
            // GTFS allows omitting agency_id when there's only one agency
            auto name_it = row.find("agency_name");
            agency.id = name_it != row.end()
                ? normalize_agency_name(name_it->second) : "default";
        }
        auto name_it = row.find("agency_name");
        if (name_it != row.end()) agency.name = name_it->second;
        auto url_it = row.find("agency_url");
        if (url_it != row.end() && !url_it->second.empty()) agency.url = url_it->second;
        auto tz_it = row.find("agency_timezone");
        if (tz_it != row.end() && !tz_it->second.empty()) agency.timezone = tz_it->second;
        auto lang_it = row.find("agency_lang");
        if (lang_it != row.end() && !lang_it->second.empty()) agency.lang = lang_it->second;
        auto phone_it = row.find("agency_phone");
        if (phone_it != row.end() && !phone_it->second.empty()) agency.phone = phone_it->second;

        if (default_agency_id.empty()) default_agency_id = agency.id;
        result.agencies.push_back(std::move(agency));
    }

    // ── Parse stops ───────────────────────────────────────────────────
    auto stop_route_map = build_stop_routes(feed.stops, feed.stop_times, feed.trips);

    for (const auto& row : feed.stops) {
        auto sid = row.find("stop_id");
        auto lat = row.find("stop_lat");
        auto lon = row.find("stop_lon");
        if (sid == row.end() || lat == row.end() || lon == row.end()) continue;

        Stop stop;
        stop.id = sid->second;
        auto name_it = row.find("stop_name");
        if (name_it != row.end()) stop.name = name_it->second;
        auto code_it = row.find("stop_code");
        if (code_it != row.end() && !code_it->second.empty()) stop.code = code_it->second;
        auto url_it = row.find("stop_url");
        if (url_it != row.end() && !url_it->second.empty()) stop.url = url_it->second;
        stop.position = {.latitude = std::stod(lat->second),
                         .longitude = std::stod(lon->second)};
        stop.stop_type = classify_stop_type(row);
        auto ps_it = row.find("parent_station");
        if (ps_it != row.end() && !ps_it->second.empty()) {
            stop.parent_stop_id = ps_it->second;
        }
        auto route_it = stop_route_map.find(sid->second);
        if (route_it != stop_route_map.end()) {
            stop.route_ids.assign(route_it->second.begin(), route_it->second.end());
        }
        result.stops.push_back(std::move(stop));
    }

    // ── Build lookup tables for route construction ────────────────────
    std::unordered_map<std::string, const CsvRow*> stop_lookup;
    for (const auto& row : feed.stops) {
        auto sid = row.find("stop_id");
        if (sid != row.end()) stop_lookup[sid->second] = &row;
    }
    std::unordered_map<std::string, int> parent_child_count;
    for (const auto& row : feed.stops) {
        auto ps = row.find("parent_station");
        if (ps != row.end() && !ps->second.empty()) {
            parent_child_count[ps->second]++;
        }
    }

    // ── Build route-stop sequences ────────────────────────────────────
    result.route_stop_sequences = build_route_stop_sequences(
        feed.stops, feed.stop_times, feed.trips);

    // Index sequences by route_id for station_sequence building
    std::unordered_map<std::string, const std::vector<std::string>*> seq_index;
    for (const auto& [rid, sids] : result.route_stop_sequences) {
        seq_index[rid] = &sids;
    }

    // ── Parse routes ──────────────────────────────────────────────────
    for (const auto& row : feed.routes) {
        auto rid = row.find("route_id");
        if (rid == row.end()) continue;

        Route route;
        route.id = rid->second;
        auto aid = row.find("agency_id");
        route.agency_id = (aid != row.end() && !aid->second.empty())
            ? aid->second : default_agency_id;
        auto sn = row.find("route_short_name");
        if (sn != row.end() && !sn->second.empty()) route.short_name = sn->second;
        auto ln = row.find("route_long_name");
        if (ln != row.end() && !ln->second.empty()) route.long_name = ln->second;
        auto rt = row.find("route_type");
        if (rt != row.end() && !rt->second.empty())
            route.route_type = std::stoi(rt->second);
        auto rc = row.find("route_color");
        if (rc != row.end() && !rc->second.empty()) route.color = rc->second;
        auto tc = row.find("route_text_color");
        if (tc != row.end() && !tc->second.empty()) route.text_color = tc->second;

        route.geometry = build_route_geom(
            route.id, feed.trips, feed.stop_times, feed.stops, feed.shapes);

        // Build station_sequence
        auto seq_it = seq_index.find(route.id);
        if (seq_it != seq_index.end()) {
            route.station_sequence = build_station_entries(
                *seq_it->second, stop_lookup, parent_child_count);
        }

        result.routes.push_back(std::move(route));
    }

    return result;
}

}  // namespace garraiobide::adapters::ingestion::gtfs
