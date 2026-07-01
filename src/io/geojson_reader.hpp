#pragma once
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace io {
    /**
     * @brief Utility to read a GeoJSON file and parse it into a JSON object.
     *
     * All methods are static; the class is not meant to be instantiated.
     */
    class GeoJsonReader {
    public:
        /**
         * @brief Read a file from disk and parse its contents as GeoJSON.
         * @param file Path to the GeoJSON file.
         * @return nlohmann::json The parsed JSON object.
         */
        [[nodiscard]] static nlohmann::json read(const std::string &file);
    };
}
