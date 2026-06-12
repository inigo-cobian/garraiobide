#include "trip.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>

#include "stop_time.hpp"

namespace gtfs {
    Trip::Trip(std::string id, std::string routeId, std::string headsign, const std::optional<int> directionId, std::string shapeId)
        : id(std::move(id)), routeId(std::move(routeId)), headsign(std::move(headsign)), directionId(directionId),
          shapeId(std::move(shapeId)) {
    }

    const std::string &Trip::get_id() const {
        return id;
    }

    const std::string &Trip::get_route_id() const {
        return routeId;
    }

    const std::string &Trip::get_headsign() const {
        return headsign;
    }

    const std::optional<int> &Trip::get_direction_id() const {
        return directionId;
    }

    const std::string &Trip::get_shape_id() const {
        return shapeId;
    }

    std::map<int, Stop> Trip::get_ordered_stops() const {
        return this->orderedStops;
    }

    void Trip::build_ordered_stops(const std::vector<Stop> &stops, const std::vector<StopTime> &stop_times) {
        auto stop_by_id = std::unordered_map<std::string, Stop>{};
        for (const auto& stop : stops) {
            stop_by_id.emplace(stop.get_id(), stop);
        }

        using std::views::filter, std::views::transform;
        auto relevant = stop_times
                      | filter([this](const StopTime& st) {
                            return st.get_trip_id() == this->id;
                        })
                      | transform([&](const StopTime& st) {
                            return std::pair{
                                st.get_stop_sequence(),
                                stop_by_id.at(st.get_stop_id())  // assumes ID exists
                            };
                        });

        //orderedStops.clear();
        std::ranges::copy(relevant, std::inserter(this->orderedStops, this->orderedStops.end()));
    }
}
