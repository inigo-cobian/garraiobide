#pragma once
#include <string>

namespace gtfs {

class StopTime {
    std::string trip_id;
    int stop_sequence;
    std::string stop_id;
    std::string location_id;
    float shape_dist_traveled;

public:
	explicit StopTime(std::string &trip_id, int stop_sequence, std::string &stop_id, std::string &location_id, float shape_dist_traveled);

    [[nodiscard]] const std::string &get_trip_id() const;

    [[nodiscard]] int get_stop_sequence() const;

    [[nodiscard]] const std::string &get_stop_id() const;

    [[nodiscard]] const std::string &get_location_id() const;

    [[nodiscard]] float get_shape_dist_traveled() const;
};

}
