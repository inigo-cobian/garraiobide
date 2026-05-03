#include "stop_time.hpp"

namespace gtfs {
    StopTime::StopTime(const std::string &trip_id, const int stop_sequence, const std::string &stop_id,
                       const std::string &location_id, const float shape_dist_traveled)
        : trip_id(trip_id), stop_sequence(stop_sequence), stop_id(stop_id), location_id(location_id),
          shape_dist_traveled(shape_dist_traveled) {
    }

    const std::string &StopTime::get_trip_id() const {
        return trip_id;
    }

    int StopTime::get_stop_sequence() const {
        return stop_sequence;
    }

    const std::string &StopTime::get_stop_id() const {
        return stop_id;
    }

    const std::string &StopTime::get_location_id() const {
        return location_id;
    }

    float StopTime::get_shape_dist_traveled() const {
        return shape_dist_traveled;
    };
}
