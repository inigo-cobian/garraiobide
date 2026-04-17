#include "shape.hpp"

#include <utility>

namespace gtfs {

Shape::Shape(std::string code, OGRLineString line) : code(std::move(code)), line(std::move(line)) {}

[[nodiscard]] OGRLineString Shape::get_line() const {
    return line;
}

} // gtfs