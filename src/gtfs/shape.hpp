#pragma once
#include <ogr_geometry.h>

namespace gtfs {
    class Shape {
        std::string code;
        OGRLineString line;

    public:
        Shape(std::string code, OGRLineString line);

        [[nodiscard]] std::string get_code() const;

        [[nodiscard]] OGRLineString get_line() const;
    };
}
