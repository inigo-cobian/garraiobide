#pragma once
#include <string>

namespace core {
    class StopInTrip {
        int order;
        std::string stopId;
        std::string tripId;
        std::string source;

    public:
        [[nodiscard]] StopInTrip(int order, const std::string &stop_id, const std::string &trip_id,
                                 const std::string &source);

        [[nodiscard]] int get_order() const;

        [[nodiscard]] std::string get_stop_id() const;

        [[nodiscard]] std::string get_trip_id() const;

        [[nodiscard]] std::string get_source() const;
    };
}
