#pragma once
#include <ogr_geometry.h>

namespace gtfs {
    class Shape {
        std::string code;
        OGRLineString line;

    public:
        [[nodiscard]] explicit Shape(std::string code, OGRLineString line);

        [[nodiscard]] const std::string &get_code() const;

        [[nodiscard]] const OGRLineString &get_line() const;
    };
}
