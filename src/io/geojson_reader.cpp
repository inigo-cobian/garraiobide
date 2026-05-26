#include "geojson_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace io {
    nlohmann::json GeoJsonReader::read(const std::string &file) {
        try {
            std::ifstream f(file);
            nlohmann::json data = nlohmann::json::parse(f);
            if (data.empty()) {
                throw std::runtime_error("Cannot parse file or empty file");
            }
            return data;
        } catch (const std::exception &e) {
            throw std::runtime_error("Could not open file " + file + ". Reason: " + e.what());
        }
    }
}
