#include "geojson_reader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace io {

void GeoJsonReader::read(const std::string& file) {
    std::ifstream f(file);
    nlohmann::json data = nlohmann::json::parse(f);
}

}