#pragma once
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace io {
    class GeoJsonReader {
    public:
        [[nodiscard]] static nlohmann::json read(const std::string &file);
    };
}
