#include "gtfs_manager.hpp"

#include <charconv>
#include <ranges>
#include "io/csv_reader.hpp"
#include "gtfs_fields.hpp"

namespace gtfs {
    void GtfsManager::load_feed(const std::string &zip_path) {
        feeds.emplace_back(zip_path);
    }

    std::vector<Stop> GtfsManager::get_stops() const {
        // TODO manage n feeds
        auto csv = feeds.at(0).get_file_content("stops.txt");
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
                            : static_cast<LocationType>(atoi(fields::stops::TYPE.c_str()));
            auto parentId = line.at(fields::stops::PARENT).empty()
                                ? std::nullopt
                                : std::make_optional(line.at(fields::stops::PARENT));

            auto stop = Stop(line.at(fields::stops::ID), line.at(fields::stops::NAME), point, type, parentId);
            stops.push_back(stop);
        }

        return stops;
    }

    std::vector<Agency> GtfsManager::get_agencies() const {
        auto csv = feeds.at(0).get_file_content("agency.txt");
        std::vector<std::string> columns = {fields::agency::ID, fields::agency::NAME};
        auto result = io::CsvReader::parse_file(csv, ',', columns);

        std::vector<Agency> agencies;
        for (auto line: result) {
            auto agency = Agency(line.at(fields::agency::ID), line.at(fields::agency::NAME));
            agencies.push_back(agency);
        }
        return agencies;
    }

    std::vector<Route> GtfsManager::get_routes() const {
        auto csv = feeds.at(0).get_file_content("routes.txt");
        std::vector<std::string> columns = {
            "route_id", "route_short_name", "route_long_name", "route_type", "route_color", "route_text_color"
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
}
