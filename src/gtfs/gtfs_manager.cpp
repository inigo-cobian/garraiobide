#include "gtfs_manager.hpp"

#include <algorithm>
#include <ranges>
#include <ogr_geometry.h>

#include <bits/ranges_algo.h>

#include "io/csv_reader.hpp"
#include "gtfs_fields.hpp"

namespace gtfs {
    void GtfsManager::load_feed(const std::string &zip_path) {
        feeds.emplace_back(zip_path);
    }

    std::vector<Stop> GtfsManager::get_stops() const {
        // TODO manage n feeds
        auto content = feeds.at(0).get_file_content("stops.txt");
        if (!content.has_value()) {
            // TODO throw error
        }
        auto csv = content.value();
        std::vector columns = {
            fields::stops::ID, fields::stops::NAME, fields::stops::LATITUDE, fields::stops::LONGITUDE,
            fields::stops::TYPE, fields::stops::PARENT
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Stop> stops;
        for (auto line: result) {
            OGRPoint point;
            if (!line.at(fields::stops::LATITUDE).empty() && !line.at(fields::stops::LONGITUDE).empty()) {
                point = OGRPoint(std::stof(line.at(fields::stops::LATITUDE)),
                                 std::stof(line.at(fields::stops::LONGITUDE)));
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
        auto content = feeds.at(0).get_file_content("agency.txt");
        if (!content.has_value()) {
            // TODO throw error
        }
        auto csv = content.value();
        std::vector columns = {fields::agency::ID, fields::agency::NAME};
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Agency> agencies;
        for (auto line: result) {
            auto agency = Agency(line.at(fields::agency::ID), line.at(fields::agency::NAME));
            agencies.push_back(agency);
        }
        return agencies;
    }

    std::vector<Route> GtfsManager::get_routes() const {
        auto content = feeds.at(0).get_file_content("routes.txt");
        if (!content.has_value()) {
            // TODO throw error
        }
        auto csv = content.value();
        std::vector columns = {
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
        auto content = feeds.at(0).get_file_content("shapes.txt");
        if (!content.has_value()) {
            // TODO throw error
            return std::nullopt;
        }
        auto csv = content.value();
        if (csv.empty()) {
            return std::nullopt;
        }
        std::vector columns = {
            fields::shapes::ID, fields::shapes::LATITUDE, fields::shapes::LONGITUDE, fields::shapes::SEQUENCE
        };
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::map<std::string, std::vector<std::pair<int, OGRPoint> > > shapes_map;
        for (auto row: result) {
            const std::string shape_id = row.at(fields::shapes::ID);
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
            shapes.push_back(Shape(shape_id, line));
        }
        return std::make_optional(shapes);
    }
}
