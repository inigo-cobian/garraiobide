#include "geojson_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace io {
    constexpr std::array validGeojsonTypes{
        "Point", "MultiPoint", "LineString", "MultiLineString",
        "Polygon", "MultiPolygon", "GeometryCollection",
        "Feature", "FeatureCollection"
    };

    nlohmann::json GeoJsonReader::read(const std::string &file) {
        std::ifstream f(file);
        try {
            f = std::ifstream(file);
        } catch (const std::exception &e) {
            throw std::runtime_error("Could not open file " + file + ". Reason: " + e.what());
        }

        nlohmann::json data = nlohmann::json::parse(f);
        if (data.empty()) {
            throw std::runtime_error("Cannot parse file or empty file");
        }
        // The GeoJSON could be validated further, but for now, let's do a simple analysis
        if (!(data.contains("type") && data.at("type").is_string())) {
            throw std::runtime_error("Cannot find type of geoJSON");
        }
        if (auto type = data["type"].get<std::string_view>(); !std::ranges::contains(validGeojsonTypes, type)) {
            throw std::runtime_error("Cannot validate type of geoJSON");
        }

        return data;
    }
}
