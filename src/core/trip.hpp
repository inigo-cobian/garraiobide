#pragma once
#include <ogr_geometry.h>
#include <string>

namespace core {
    /**
     * @brief Represents a transit trip (a single journey of a line).
     *
     * Holds an ID, a polyline shape (as an OGRLineString), and a source.
     */
    class Trip {
        std::string id;
        OGRLineString shape;
        std::string source;

    public:
        /**
         * @brief Construct a new Trip.
         * @param id Unique trip identifier.
         * @param shape Geographic shape of the trip (sequence of points).
         * @param source Source identifier.
         */
        [[nodiscard]] Trip(const std::string &id, const OGRLineString &shape, const std::string &source);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] OGRLineString get_shape() const;

        [[nodiscard]] std::string get_source() const;
    };
}
