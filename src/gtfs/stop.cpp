#include "stop.hpp"

#include <utility>

namespace gtfs {
    Stop::Stop(std::string id, std::string name, float latitude, float longitude)
        : id(std::move(id)), name(std::move(name)), point(latitude, longitude) {
    }

    Stop::Stop(std::string id, std::string name, OGRPoint point)
        : id(std::move(id)), name(std::move(name)), point(std::move(point)) {
    }

    [[nodiscard]] std::string Stop::get_id() const {
        return id;
    }

    [[nodiscard]] std::string Stop::get_name() const {
        return name;
    }

    [[nodiscard]] OGRPoint Stop::get_point() const {
        return point;
    }
}
