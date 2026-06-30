#pragma once
#include <ogr_geometry.h>
#include <string>

namespace core {
    class Trip {
        std::string id;
        OGRLineString shape;
        std::string source;

    public:
        [[nodiscard]] Trip(const std::string &id, const OGRLineString &shape, const std::string &source);

        [[nodiscard]] std::string get_id() const;

        [[nodiscard]] OGRLineString get_shape() const;

        [[nodiscard]] std::string get_source() const;
    };
}
