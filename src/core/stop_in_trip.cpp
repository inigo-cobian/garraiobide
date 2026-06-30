#include "stop_in_trip.hpp"

namespace core {
    StopInTrip::StopInTrip(const int order, const std::string &stop_id, const std::string &line_id,
                           const std::string &source)
        : order(order),
          stopId(stop_id),
          tripId(line_id),
          source(source) {
    }

    int StopInTrip::get_order() const {
        return order;
    }

    std::string StopInTrip::get_stop_id() const {
        return stopId;
    }

    std::string StopInTrip::get_trip_id() const {
        return tripId;
    }

    std::string StopInTrip::get_source() const {
        return source;
    }
}
