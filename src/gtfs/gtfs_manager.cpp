#include "gtfs_manager.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <ogr_geometry.h>
#include <unordered_set>

#include <bits/ranges_algo.h>

#include "io/csv_reader.hpp"
#include "gtfs_fields.hpp"
#include "gtfs_files.hpp"
#include "stop_time.hpp"

namespace gtfs {
    void GtfsManager::load_feed(const std::string &zip_path) {
        feeds.emplace_back(zip_path);
    }

    std::vector<Stop> GtfsManager::get_stops() const {
        // TODO manage n feeds
        auto content = feeds.at(0).get_file_content(files::STOPS);
        if (!content.has_value()) {
            // TODO throw error
        }
        const auto& csv = content.value();
        const std::vector columns = {
            fields::stops::ID, fields::stops::NAME, fields::stops::LATITUDE, fields::stops::LONGITUDE,
            fields::stops::TYPE, fields::stops::PARENT
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Stop> stops;
        for (auto line: result) {
            OGRPoint point;
            if (!line.at(fields::stops::LATITUDE).empty() && !line.at(fields::stops::LONGITUDE).empty()) {
                try {
                    point = OGRPoint(std::stof(line.at(fields::stops::LATITUDE)),
                    std::stof(line.at(fields::stops::LONGITUDE)));
                } catch (...) {
                    point.empty();
                }
            }
            auto type = line.at(fields::stops::TYPE).empty()
                            ? LocationType::Stop
                            // FIXME
                            : LocationType::Entrance; //static_cast<LocationType>(std::stoi(fields::stops::TYPE));
            auto parentId = line.at(fields::stops::PARENT).empty()
                                ? std::nullopt
                                : std::make_optional(line.at(fields::stops::PARENT));

            auto stop = Stop(line.at(fields::stops::ID), line.at(fields::stops::NAME), point, type, parentId);
            stops.push_back(stop);
        }

        return stops;
    }

    std::vector<Agency> GtfsManager::get_agencies() const {
        auto content = feeds.at(0).get_file_content(files::AGENCY);
        if (!content.has_value()) {
            // TODO throw error
        }
        const auto& csv = content.value();
        const std::vector columns = {fields::agency::ID, fields::agency::NAME};
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Agency> agencies;
        for (auto line: result) {
            auto agency = Agency(line.at(fields::agency::ID), line.at(fields::agency::NAME));
            agencies.push_back(agency);
        }
        return agencies;
    }

    std::vector<Route> GtfsManager::get_routes() const {
        auto content = feeds.at(0).get_file_content(files::ROUTES);
        if (!content.has_value()) {
            // TODO throw error
        }
        const auto& csv = content.value();
        const std::vector columns = {
            fields::routes::ID, fields::routes::SHORT_NAME, fields::routes::LONG_NAME, fields::routes::TYPE,
            fields::routes::COLOR, fields::routes::TEXT_COLOR
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Route> routes;
        for (auto line: result) {
            std::string name = line.at(fields::routes::LONG_NAME).empty()
                                   ? line.at(fields::routes::SHORT_NAME)
                                   : line.at(fields::routes::LONG_NAME);
            auto route = Route(line.at(fields::routes::ID), name, line.at(fields::routes::TYPE),
                               line.at(fields::routes::COLOR), line.at(fields::routes::TEXT_COLOR));
            routes.push_back(route);
        }
        return routes;
    }

    std::optional<std::vector<Shape> > GtfsManager::get_shapes() const {
        auto content = feeds.at(0).get_file_content(files::SHAPES);
        if (!content.has_value()) {
            // TODO throw error
            return std::nullopt;
        }
        const auto& csv = content.value();
        if (csv.empty()) {
            return std::nullopt;
        }
        const std::vector columns = {
            fields::shapes::ID, fields::shapes::LATITUDE, fields::shapes::LONGITUDE, fields::shapes::SEQUENCE
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::map<std::string, std::vector<std::pair<int, OGRPoint> > > shapes_map;
        for (auto row: result) {
            const std::string& shape_id = row.at(fields::shapes::ID);
            const int sequence = atoi(row.at(fields::shapes::SEQUENCE).c_str());
            const double lat = std::stod(row.at(fields::shapes::LATITUDE));
            const double lon = std::stod(row.at(fields::shapes::LONGITUDE));

            OGRPoint point(lon, lat);
            shapes_map[shape_id].emplace_back(sequence, point);
        }

        std::vector<Shape> shapes;
        for (auto &[shape_id, points_with_seq]: shapes_map) {
            std::ranges::sort(points_with_seq,
                              [](const auto &a, const auto &b) { return a.first < b.first; });

            std::vector<OGRPoint> ordered_points;
            ordered_points.reserve(points_with_seq.size());
            for (auto &[seq, pt]: points_with_seq) {
                ordered_points.push_back(pt);
            }
            OGRLineString line;
            for (auto &pt: ordered_points) {
                line.addPoint(&pt);
            }
            shapes.emplace_back(shape_id, line);
        }
        return std::make_optional(shapes);
    }

    std::vector<Trip> GtfsManager::get_trips() const {
        const auto content = feeds.at(0).get_file_content(files::TRIPS);
        if (!content.has_value()) {
            // TODO throw error
            return {};
        }
        const auto& csv = content.value();
        if (csv.empty()) {
            return {};
        }

        const std::vector columns = {
            fields::trips::ID, fields::trips::ROUTE_ID, fields::trips::HEADSIGN, fields::trips::DIRECTION_ID,
            fields::trips::SHAPE_ID
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Trip> trips;
        for (auto line: result) {
            std::optional<int> directionId = line.at(fields::trips::DIRECTION_ID).empty()
                ? std::nullopt
                : std::make_optional(std::stoi(line.at(fields::trips::DIRECTION_ID)));
            auto trip = Trip(line.at(fields::trips::ID), line.at(fields::trips::ROUTE_ID),
                             line.at(fields::trips::HEADSIGN),
                             directionId, line.at(fields::trips::SHAPE_ID));
            trips.push_back(trip);
        }

        return trips;
    }

    std::vector<StopTime> GtfsManager::get_stop_times() const {
        auto content = feeds.at(0).get_file_content(files::STOP_TIMES);
        if (!content.has_value()) {
            // TODO throw error
            return {};
        }
        const auto& csv = content.value();
        if (csv.empty()) {
            return {};
        }

        const std::vector columns = {
            fields::stop_times::TRIP_ID, fields::stop_times::STOP_ID, fields::stop_times::LOCATION_ID,
            fields::stop_times::STOP_SEQUENCE, fields::stop_times::STOP_HEADSIGN,
            fields::stop_times::SHAPE_DIST_TRAVELED
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<StopTime> stop_times;
        for (auto line: result) {
            auto dist_travelled = line.at(fields::stop_times::SHAPE_DIST_TRAVELED).empty()
                                      ? NAN
                                      : std::stof(line.at(fields::stop_times::SHAPE_DIST_TRAVELED));

            stop_times.emplace_back(line.at(fields::stop_times::TRIP_ID),
                                    std::stoi(line.at(fields::stop_times::STOP_SEQUENCE)),
                                    line.at(fields::stop_times::STOP_ID),
                                    line.at(fields::stop_times::LOCATION_ID),
                                    dist_travelled);
        }
        return stop_times;
    }

}
