#include "trip.hpp"

namespace core {
    Trip::Trip(const std::string &id, const OGRLineString &shape, const std::string &source)
        : id(id), shape(shape), source(source) {
    }

    std::string Trip::get_id() const {
        return id;
    }

    OGRLineString Trip::get_shape() const {
        return shape;
    }

    std::string Trip::get_source() const {
        return source;
    }
}
