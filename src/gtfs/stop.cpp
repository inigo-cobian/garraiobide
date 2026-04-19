#include "stop.hpp"

#include <utility>

namespace gtfs {
    Stop::Stop(std::string id, std::string name, float latitude, float longitude, LocationType type,
               std::optional<std::string> parent)
        : id(std::move(id)), name(std::move(name)), point(latitude, longitude), locationType(type),
          parentStation(std::move(parent)) {
    }

    Stop::Stop(std::string id, std::string name, float latitude, float longitude, int type,
               std::optional<std::string> parent)
        : id(std::move(id)), name(std::move(name)), point(latitude, longitude),
          locationType(static_cast<LocationType>(type)),
          parentStation(std::move(parent)) {
    }

    Stop::Stop(std::string id, std::string name, OGRPoint point, LocationType type,
               std::optional<std::string> parent)
        : id(std::move(id)), name(std::move(name)), point(std::move(point)), locationType(type),
          parentStation(std::move(parent)) {
    }

    Stop::Stop(std::string id, std::string name, OGRPoint point, int type,
               std::optional<std::string> parent)
        : id(std::move(id)), name(std::move(name)), point(std::move(point)),
          locationType(static_cast<LocationType>(type)),
          parentStation(std::move(parent)) {
    }

    [[nodiscard]] const std::string &Stop::get_id() const {
        return id;
    }

    [[nodiscard]] const std::string &Stop::get_name() const {
        return name;
    }

    [[nodiscard]] const OGRPoint &Stop::get_point() const {
        return point;
    }

    [[nodiscard]] const LocationType &Stop::location_type() const {
        return locationType;
    }

    [[nodiscard]] const std::optional<std::string> &Stop::parent_station() const {
        return parentStation;
    }
}
