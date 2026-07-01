#pragma once

#include <pqxx/pqxx>
#include <string>

namespace core {
    class Stop;
    class Line;
    class StopInTrip;
    class Trip;
}

namespace data {
    /**
     * @brief Manages interactions with a PostGIS database.
     *
     * Provides methods to insert stops, lines, stop‑in‑trip associations, and trips,
     * as well as existence checks. On construction, it enables the PostGIS extension
     * and creates the necessary tables if they do not exist.
     */
    class PostgisManager {
    public:
        /**
         * @brief Construct a new PostgisManager and set up the schema.
         * @param connection_string libpq‑style connection string (e.g. "host=... dbname=...").
         */
        explicit PostgisManager(const std::string &connection_string);

        void insertStop(const core::Stop &stop) const;

        void insertLine(const core::Line &line) const;

        void insertStopInTrip(const core::StopInTrip &stop_in_trip) const;

        /**
         * @brief Associate a trip with a line.
         * @param line_id ID of the line.
         * @param line_source Source of the line.
         * @param trip_id ID of the trip.
         * @param trip_source Source of the trip.
         */
        void insertTripInLine(const std::string &line_id,
                              const std::string &line_source,
                              const std::string &trip_id,
                              const std::string &trip_source) const;

        void insertTrip(const core::Trip &trip) const;

        /**
         * @brief Check if a stop exists in the database.
         * @param stop_id Stop identifier.
         * @param source Source identifier.
         * @return true if the stop exists, false otherwise.
         */
        [[nodiscard]] bool stopExists(const std::string &stop_id, const std::string &source) const;

        /**
         * @brief Check if a line exists.
         * @param line_id Line identifier.
         * @param source Source identifier.
         * @return true if the line exists.
         */
        [[nodiscard]] bool lineExists(const std::string &line_id, const std::string &source) const;

        /**
         * @brief Check if a trip exists.
         * @param trip_id Trip identifier.
         * @param source Source identifier.
         * @return true if the trip exists.
         */
        [[nodiscard]] bool tripExists(const std::string &trip_id, const std::string &source) const;

    private:
        mutable pqxx::connection conn_;

        void createTables();

        void enablePostGis();
    };
}
