#include "stop.hpp"

namespace core {
    Stop::Stop(const std::string &id, const std::string &name, const double longitude, const double latitude,
               const std::string &source)
        : id(id),
          name(name),
          longitude(longitude),
          latitude(latitude),
          source(source) {
    }

    Stop::Stop(Stop &&other) noexcept
        : id(std::move(other.id)),
          name(std::move(other.name)),
          longitude(other.longitude),
          latitude(other.latitude),
          source(std::move(other.source)) {
    }

    [[nodiscard]] const std::string &Stop::get_id() const {
        return id;
    }

    [[nodiscard]] const std::string &Stop::get_name() const {
        return name;
    }

    [[nodiscard]] double Stop::get_longitude() const {
        return longitude;
    }

    [[nodiscard]] double Stop::get_latitude() const {
        return latitude;
    }

    [[nodiscard]] const std::string &Stop::get_source() const {
        return source;
    }
}
