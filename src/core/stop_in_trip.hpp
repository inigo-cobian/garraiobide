#pragma once
#include <string>

namespace core {
    /**
     * @brief Represents the association of a Stop with a Trip at a specific order.
     *
     * Used to model stop sequences (from stop times) within a trip.
     */
    class StopInTrip {
        int order;
        std::string stopId;
        std::string tripId;
        std::string source;

    public:
        /**
         * @brief Construct a StopInTrip relation.
         * @param order Sequence number.
         * @param stop_id ID of the stop.
         * @param trip_id ID of the trip.
         * @param source Source identifier.
         */
        [[nodiscard]] StopInTrip(int order, const std::string &stop_id, const std::string &trip_id,
                                 const std::string &source);

        [[nodiscard]] int get_order() const;

        [[nodiscard]] std::string get_stop_id() const;

        [[nodiscard]] std::string get_trip_id() const;

        [[nodiscard]] std::string get_source() const;
    };
}
