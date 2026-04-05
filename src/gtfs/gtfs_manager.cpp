#include "gtfs_manager.hpp"

#include "io/csv_reader.hpp"

namespace gtfs {

void GtfsManager::load_feed(const std::string& zip_path) {
    feeds.emplace_back(zip_path);
}

std::vector<Stop> GtfsManager::get_stops() const {
    // TODO manage n feeds
    auto csv = feeds.at(0).get_file_content("stops.txt");
    std::vector<std::string> columns = {"stop_id", "stop_name", "stop_lat", "stop_lon", "location_type", "parent_station"};
    auto result = io::CsvReader::parse_file(csv, ',', columns);

    std::vector<Stop> stops;
    for (auto line : result) {
        auto stop = Stop(line.at(0), line.at(1), std::stof(line.at(2)), std::stof(line.at(3)));
        stops.push_back(stop);
    }

    return stops;
}

std::vector<Agency> GtfsManager::get_agencies() const {
    auto csv = feeds.at(0).get_file_content("agency.txt");
    std::vector<std::string> columns = {"agency_id", "agency_name"};
    auto result = io::CsvReader::parse_file(csv, ',', columns);

    std::vector<Agency> agencies;
    for (auto line : result) {
        auto agency = Agency(line.at(0), line.at(1));
        agencies.push_back(agency);
    }
    return agencies;
}
}
