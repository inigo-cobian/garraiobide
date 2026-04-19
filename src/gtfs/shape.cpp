#include "shape.hpp"

#include <utility>

namespace gtfs {
    Shape::Shape(std::string code, OGRLineString line) : code(std::move(code)), line(std::move(line)) {
    }

    const std::string &Shape::get_code() const {
        return code;
    }

    const OGRLineString &Shape::get_line() const {
        return line;
    }
}
