#include "shape.hpp"

namespace gtfs {

Shape::Shape(std::string code, OGRLineString line) : code(code), line(std::move(line)) {}

OGRLineString Shape::get_line() const {
    return line;
}

} // gtfs