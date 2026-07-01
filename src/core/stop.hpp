#pragma once
#include <string>

#include "gtfs/stop.hpp"

namespace core {
    /**
     * @brief Represents a public transit stop.
     *
     * Contains an identifier, name, geographic coordinates, and a source
     * (a.k.a. the resource name). Can be constructed from either raw fields
     * or a gtfs::Stop object.
     */
    class Stop {
        std::string id;
        std::string name;
        double longitude;
        double latitude;
        std::string source;
        // TODO entrances/exits
        // TODO isochrones

    public:
        /**
         * @brief Construct from individual fields.
         * @param id Unique stop ID.
         * @param name Display name.
         * @param longitude Longitude in degrees.
         * @param latitude Latitude in degrees.
         * @param source Source identifier.
         */
        Stop(const std::string &id, const std::string &name, double longitude, double latitude,
             const std::string &source);

        /**
         * @brief Construct from a GTFS stop object.
         * @param gtfs_stop The GTFS stop.
         * @param source Source identifier.
         */
        Stop(gtfs::Stop gtfs_stop, const std::string &source);

        Stop(Stop &&other) noexcept;

        [[nodiscard]] const std::string &get_id() const;

        [[nodiscard]] const std::string &get_name() const;

        [[nodiscard]] double get_longitude() const;

        [[nodiscard]] double get_latitude() const;

        [[nodiscard]] const std::string &get_source() const;

        Stop(const Stop &other) = default;

        ~Stop() = default;
    };
}
